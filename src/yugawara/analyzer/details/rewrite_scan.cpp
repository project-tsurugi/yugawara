#include "rewrite_scan.h"

#include <glog/logging.h>

#include <optional>

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
                if (!scan.limit() && !scan.lower() && !scan.upper()) {
                    process(scan);
                }
            }
        }
        install(graph);
    }

private:
    index_estimator const& index_estimator_;
    scan_key_collector collector_;

    relation::graph_type work_graph_ {};

    std::vector<scan_key_collector::term*> term_buf_;
    std::vector<search_key> search_key_buf_;

    bool saw_best_ { false };
    std::optional<index_estimator::result> saved_result_ {};
    optional_ptr<storage::index const> saved_index_ {};
    std::vector<scan_key_collector::term*> saved_terms_;

    void process(relation::scan& expr) {
        if (!collector_(expr, false)) {
            return;
        }
        auto&& storages = collector_.table().owner();
        if (!storages) {
            // there is no index information
            return;
        }
        storages->each_table_index(
                collector_.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    estimate(*entry);
                });
        if (saved_index_) {
            apply_result(expr, *saved_index_, saved_terms_);
        }

        // clear the round data
        collector_.clear();
        term_buf_.clear();
        search_key_buf_.clear();
        saw_best_ = {};
        saved_index_ = {};
        saved_result_ = {};
        saved_terms_.clear();
    }

    void install(relation::graph_type& graph) {
        relation::merge_into(std::move(work_graph_), graph);
        work_graph_.clear();

        remove_orphaned_elements(graph);
    }

    void estimate(storage::index const& index) {
        if (saw_best_) {
            return;
        }
        build_search_key(index);
        auto result = index_estimator_(
                index,
                search_key_buf_,
                collector_.sort_keys(),
                collector_.referring());

        VLOG(50) << "index " << index.simple_name() << ": " << result;
        if (is_better(result)) {
            save_result(index, result, term_buf_);
        }
        term_buf_.clear();
        search_key_buf_.clear();
    }

    void build_search_key(storage::index const& index) {
        BOOST_ASSERT(term_buf_.empty()); // NOLINT
        BOOST_ASSERT(search_key_buf_.empty()); // NOLINT

        auto&& keys = index.keys();
        term_buf_.reserve(keys.size());
        search_key_buf_.reserve(keys.size());

        for (auto&& key : keys) {
            if (auto term = collector_.find(key.column())) {
                BOOST_ASSERT(term); // NOLINT

                term_buf_.emplace_back(term.get());
                search_key_buf_.emplace_back(term->build_index_search_key(key.column()));

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

    bool is_better(index_estimator::result const& result) {
        if (!saved_result_) {
            return true;
        }
        auto&& saved = *saved_result_;
        if (!saved.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::single_row)) {
            return true;
        }
        return saved.score() < result.score();
    }

    void save_result(
            storage::index const& index,
            index_estimator::result result,
            sequence_view<scan_key_collector::term*> terms) {
        if (result.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::index_only)) {
            saw_best_ = true;
        }
        saved_index_ = index;
        saved_result_.emplace(result);
        saved_terms_.assign(terms.begin(), terms.end());
    }

    void apply_result(
            relation::scan& expr,
            storage::index const& index,
            sequence_view<scan_key_collector::term*> terms) {
        auto&& result = *saved_result_;
        if (result.attributes().contains(attribute::find)) {
            rewrite_as_find(expr, index, terms);
            return;
        }
        rewrite_bounds(expr, index, terms);
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
