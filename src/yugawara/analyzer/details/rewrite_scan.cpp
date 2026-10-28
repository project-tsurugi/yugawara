#include "rewrite_scan.h"

#include <glog/logging.h>

#include <optional>
#include <utility>
#include <vector>

#include <takatori/relation/scan.h>
#include <takatori/relation/find.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/provider.h>

#include "estimate_index.h"
#include "scan_key_collector.h"
#include "remove_orphaned_elements.h"
#include "rewrite_criteria.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using attribute = details::index_estimator_result_attribute;

using ::takatori::util::sequence_view;
using ::takatori::util::unsafe_downcast;

namespace {

class engine {
public:
    explicit engine(index_estimator const& index_estimator) :
        index_estimator_ { index_estimator }
    {}

    void process(relation::graph_type& graph) {
        work_graph_.clear();
        for (auto&& expr : graph) {
            if (expr.kind() == relation::scan::tag) {
                auto&& scan = unsafe_downcast<relation::scan>(expr);
                if (!scan.row_slice() && !scan.lower() && !scan.upper()) {
                    process(scan);
                }
            }
        }
        install(graph);
    }

private:
    index_estimator const& index_estimator_;

    relation::graph_type work_graph_ {};

    void process(relation::scan& expr) {
        // NOTE: the collector and the candidate are only valid while processing the current scan
        scan_key_collector collector {};
        if (!collector(expr, scan_key_collector::scope::scan)) {
            return;
        }
        auto&& storages = collector.table().owner();
        if (!storages) {
            // there is no index information
            return;
        }
        std::optional<index_candidate> saved {};
        storages->each_table_index(
                collector.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    if (saved && is_best(*saved)) {
                        return;
                    }
                    auto current = estimate_index(collector, *entry, index_estimator_);
                    VLOG(50) << "index " << entry->simple_name() << ": " << current.result;
                    if (is_better(saved, current)) {
                        saved = std::move(current);
                    }
                });
        if (saved) {
            apply_result(expr, *saved);
        }
    }

    void install(relation::graph_type& graph) {
        relation::merge_into(std::move(work_graph_), graph);
        work_graph_.clear();

        remove_orphaned_elements(graph);
    }

    static bool is_best(index_candidate const& candidate) {
        auto&& attributes = candidate.result.attributes();
        return attributes.contains(attribute::single_row)
                && attributes.contains(attribute::index_only);
    }

    static bool is_better(std::optional<index_candidate> const& saved, index_candidate const& current) {
        if (!saved) {
            return true;
        }
        auto&& saved_result = saved->result;
        auto&& result = current.result;
        if (!saved_result.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::single_row)) {
            return true;
        }
        return saved_result.score() < result.score();
    }

    void apply_result(relation::scan& expr, index_candidate& saved) {
        auto&& index = saved.index.get();
        if (saved.result.attributes().contains(attribute::find)) {
            rewrite_as_find(expr, index, saved.terms);
            return;
        }
        rewrite_bounds(expr, index, saved.terms);
    }

    void rewrite_as_find(
            relation::scan& expr,
            storage::index const& index,
            sequence_view<scan_key_collector::term*> terms) {
        BOOST_ASSERT(terms.size() <= index.keys().size()); // NOLINT

        std::vector<relation::find::key> keys {};
        fill_search_key(index, keys, terms);

        binding::factory bindings {};
        auto&& find = work_graph_.emplace<relation::find>(
                bindings(index),
                std::move(expr.columns()),
                std::move(keys));

        // reconnect
        BOOST_ASSERT(expr.output().opposite()); // NOLINT
        auto&& downstream = *expr.output().opposite();
        expr.output().disconnect_all();
        find.output().connect_to(downstream);
    }

    void rewrite_bounds(
            relation::scan& expr,
            storage::index const& index,
            sequence_view<scan_key_collector::term*> terms) const {
        BOOST_ASSERT(terms.size() <= index.keys().size()); // NOLINT

        binding::factory bindings {};
        expr.source() = bindings(index);

        fill_endpoints(index, expr.lower(), expr.upper(), terms);
    }
};

} // namespace

void rewrite_scan(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator) {
    engine e { index_estimator };
    e.process(graph);
}

} // namespace yugawara::analyzer::details
