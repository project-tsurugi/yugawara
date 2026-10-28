#include "rewrite_scan.h"

#include <glog/logging.h>

#include <optional>
#include <utility>
#include <vector>

#include <takatori/relation/scan.h>
#include <takatori/relation/find.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/provider.h>

#include "scan_key_collector.h"
#include "remove_orphaned_elements.h"
#include "rewrite_criteria.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using search_key = storage::details::search_key_element;
using attribute = details::index_estimator_result_attribute;

using ::takatori::util::optional_ptr;
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
    /**
     * @brief an index candidate for the current scan.
     * @details The terms point to the elements in the scan_key_collector of the current scan,
     *      so that this must not outlive it.
     */
    struct candidate {
        optional_ptr<storage::index const> index {};
        std::optional<index_estimator::result> result {};
        std::vector<scan_key_collector::term*> terms {};
        bool best { false };
    };

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
        candidate saved {};
        storages->each_table_index(
                collector.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    if (saved.best) {
                        return;
                    }
                    auto current = estimate(collector, *entry);
                    if (is_better(saved, *current.result)) {
                        saved = std::move(current);
                    }
                });
        if (saved.index) {
            apply_result(expr, saved);
        }
    }

    void install(relation::graph_type& graph) {
        relation::merge_into(std::move(work_graph_), graph);
        work_graph_.clear();

        remove_orphaned_elements(graph);
    }

    candidate estimate(scan_key_collector& collector, storage::index const& index) const {
        candidate result {};
        result.index = index;

        std::vector<search_key> search_keys {};
        build_search_key(collector, index, result.terms, search_keys);
        auto&& estimated = result.result.emplace(index_estimator_(
                index,
                search_keys,
                collector.sort_keys(),
                collector.referring()));

        VLOG(50) << "index " << index.simple_name() << ": " << estimated;
        result.best = estimated.attributes().contains(attribute::single_row)
                && estimated.attributes().contains(attribute::index_only);
        return result;
    }

    static void build_search_key(
            scan_key_collector& collector,
            storage::index const& index,
            std::vector<scan_key_collector::term*>& terms,
            std::vector<search_key>& search_keys) {
        auto&& keys = index.keys();
        terms.reserve(keys.size());
        search_keys.reserve(keys.size());

        for (auto&& key : keys) {
            if (auto term = collector.find(key.column())) {
                BOOST_ASSERT(term); // NOLINT

                terms.emplace_back(term.get());
                search_keys.emplace_back(term->build_index_search_key(key.column()));

                // FIXME: support more complex conditions, with pre/post filters
                if (!term->equivalent()) {
                    break;
                }
            } else {
                // no more keys
                break;
            }
        }
    }

    static bool is_better(candidate const& saved, index_estimator::result const& result) {
        if (!saved.result) {
            return true;
        }
        auto&& saved_result = *saved.result;
        if (!saved_result.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::single_row)) {
            return true;
        }
        return saved_result.score() < result.score();
    }

    void apply_result(relation::scan& expr, candidate& saved) {
        auto&& index = *saved.index;
        if (saved.result->attributes().contains(attribute::find)) {
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
