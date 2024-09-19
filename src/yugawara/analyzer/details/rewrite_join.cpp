#include "rewrite_join.h"

#include <optional>

#include <takatori/scalar/binary.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/relation/filter.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/finalizer.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/provider.h>

#include "boolean_constants.h"
#include "scan_key_collector.h"
#include "remove_orphaned_elements.h"
#include "rewrite_criteria.h"

namespace yugawara::analyzer::details {

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using search_key = storage::details::search_key_element;
using attribute = details::index_estimator_result_attribute;

using ::takatori::relation::join_kind;
using ::takatori::relation::join_kind_set;
using ::takatori::util::ownership_reference;
using ::takatori::util::optional_ptr;
using ::takatori::util::sequence_view;
using ::takatori::util::unsafe_downcast;
using ::takatori::util::finalizer;

using expression_ref = ownership_reference<scalar::expression>;

namespace {

class engine {
public:
    explicit engine(
            index_estimator const& index_estimator,
            flow_volume_info const& flow_volume,
            bool allow_join_scan) :
        index_estimator_ { index_estimator },
        flow_volume_ { flow_volume },
        allow_join_scan_ { allow_join_scan }
    {}

    void process(relation::graph_type& graph) {
        work_graph_.clear();
        for (auto&& expr : graph) {
            if (expr.kind() == relation::intermediate::join::tag) {
                auto&& join = unsafe_downcast<relation::intermediate::join>(expr);
                if (!join.lower() && !join.upper()) {
                    process(join);
                }
            }
        }
        install(graph);
    }

private:
    index_estimator const& index_estimator_;
    flow_volume_info const& flow_volume_;
    bool allow_join_scan_;

    relation::graph_type work_graph_ {};

    scan_key_collector left_collector_;
    scan_key_collector right_collector_;

    std::vector<search_key_term*> term_buf_;
    std::vector<search_key> search_key_buf_;
    std::vector<relation::filter*> filter_buf_;

    optional_ptr<relation::scan> saved_scan_ {};
    optional_ptr<storage::index const> saved_index_ {};
    bool saved_left_ {};
    std::optional<index_estimator::result> saved_result_ {};
    std::vector<search_key_term*> saved_terms_;
    std::vector<relation::filter*> saved_filters_;

    void process(relation::intermediate::join& expr) {
        process_right(expr);
        process_left(expr);
        if (saved_scan_) {
            auto condition = apply_result(expr, *saved_scan_, *saved_index_, saved_left_, saved_terms_);
            merge_filters(std::move(condition), saved_filters_);
        }

        // clear the round data
        left_collector_.clear();
        right_collector_.clear();
        term_buf_.clear();
        search_key_buf_.clear();
        filter_buf_.clear();
        saved_scan_ = {};
        saved_index_ = {};
        saved_left_ = {};
        saved_result_ = {};
        saved_terms_.clear();
        saved_filters_.clear();
    }

    void install(relation::graph_type& graph) {
        relation::merge_into(std::move(work_graph_), graph);
        work_graph_.clear();

        remove_orphaned_elements(graph);
    }

    void process_left(relation::intermediate::join& expr) {
        static constexpr join_kind_set allow {
                join_kind::inner,
        };
        if (!allow.contains(expr.operator_kind())) {
            return;
        }
        auto&& collector = left_collector_;
        auto scan = find_scan(expr.left(), false);
        finalizer filter_buf_finalizer {
                [&]() -> void {
                    filter_buf_.clear();
                }
        };
        if (!scan || !collector(*scan, true)) {
            return;
        }
        auto storages = collector.table().owner();
        if (!storages) {
            // there are no index information
            return;
        }
        storages->each_table_index(
                collector.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    estimate(expr, *scan, *entry, collector, true);
                });
        filter_buf_.clear();
    }

    void process_right(relation::intermediate::join& expr) {
        static constexpr join_kind_set allow {
                join_kind::inner,
                join_kind::semi,
                join_kind::anti,
                join_kind::left_outer,
        };
        static constexpr join_kind_set direct {
                join_kind::anti,
                join_kind::left_outer,
        };
        if (!allow.contains(expr.operator_kind())) {
            return;
        }
        auto&& collector = right_collector_;
        auto scan = find_scan(expr.right(), direct.contains(expr.operator_kind()));
        finalizer filter_buf_finalizer {
            [&]() -> void {
                filter_buf_.clear();
            }
        };
        if (!scan || !collector(*scan, true)) {
            return;
        }
        auto storages = collector.table().owner();
        if (!storages) {
            // there are no index information
            return;
        }
        storages->each_table_index(
                collector.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    estimate(expr, *scan, *entry, collector, false);
                });
        filter_buf_.clear();
    }

    optional_ptr<relation::scan> find_scan(relation::expression::input_port_type& input, bool direct) {
        BOOST_ASSERT(filter_buf_.empty()); // NOLINT
        auto current = input.opposite();
        while (true) {
            BOOST_ASSERT(current); // NOLINT
            auto&& expr = current->owner();
            using kind = relation::expression_kind;
            switch (expr.kind()) {
                case kind::scan: {
                    auto&& result = unsafe_downcast<relation::scan>(expr);
                    if (result.lower() || result.upper() || result.limit()) {
                        return {};
                    }
                    return result;
                }

                case kind::filter: {
                    if (direct) {
                        return {};
                    }
                    auto&& filter = unsafe_downcast<relation::filter>(expr);
                    filter_buf_.emplace_back(std::addressof(filter));
                    current = filter.input().opposite();
                    break; // continue to upstream
                }

                default:
                    return {};
            }
        }
    }

    void estimate(
            relation::intermediate::join const& expr,
            relation::scan& scan,
            storage::index const& index,
            scan_key_collector& collector,
            bool left) {
        build_search_key(index, collector);
        if (search_key_buf_.empty()) {
            // index based join must have at least one term
            return;
        }
        finalizer f {
            [&]() -> void {
                term_buf_.clear();
                search_key_buf_.clear();
            }
        };
        auto result = index_estimator_(
                index,
                search_key_buf_,
                collector.sort_keys(),
                collector.referring());

        if (!result.attributes().contains(attribute::find) &&
                !(allow_join_scan_ && result.attributes().contains(attribute::range_scan))) {
            // no accepted methods
            return;
        }

        // FIXME: use more statistics of individual join inputs
        if (is_better(expr, result, left)) {
            save_result(scan, index, left, result, term_buf_, filter_buf_);
        }
    }

    void build_search_key(storage::index const& index, scan_key_collector& collector) {
        BOOST_ASSERT(term_buf_.empty()); // NOLINT
        BOOST_ASSERT(search_key_buf_.empty()); // NOLINT

        auto&& keys = index.keys();
        term_buf_.reserve(keys.size());
        search_key_buf_.reserve(keys.size());

        for (auto&& key : keys) {
            if (auto term = collector.find(key.column())) {
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

    static relation::expression::input_port_type const& input(relation::intermediate::join const& expr, bool left) {
        return left ? expr.left() : expr.right();
    }

    bool is_better(relation::intermediate::join const& expr, index_estimator::result const& result, bool left) {
        if (!saved_result_) {
            return true;
        }
        auto&& saved = *saved_result_;
        if (!saved.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::single_row)) {
            return true;
        }
        auto saved_vol = flow_volume_.find(input(expr, saved_left_));
        auto result_vol = flow_volume_.find(input(expr, left));
        if (saved_vol && result_vol) {
            auto saved_score = saved.score() * saved_vol->row_count * saved_vol->column_size;
            auto result_score = result.score() * result_vol->row_count * result_vol->column_size;
            return saved_score < result_score;
        }
        return saved.score() < result.score();
    }

    void save_result(
            relation::scan& expr,
            storage::index const& index,
            bool left,
            index_estimator::result result,
            sequence_view<scan_key_collector::term*> terms,
            sequence_view<relation::filter*> filters) {
        saved_scan_ = expr;
        saved_index_ = index;
        saved_left_ = left;
        saved_result_.emplace(result);
        saved_terms_.assign(terms.begin(), terms.end());
        saved_filters_.assign(filters.begin(), filters.end());
    }

    expression_ref apply_result(
            relation::intermediate::join& expr,
            relation::scan& source,
            storage::index const& index,
            bool left,
            sequence_view<scan_key_collector::term*> terms) {
        if (left) {
            swap_upstream(expr.left(), expr.right());
        }
        auto&& result = *saved_result_;
        if (result.attributes().contains(attribute::find)) {
            return rewrite_as_join_find(expr, source, index, terms);
        }
        return rewrite_as_join_scan(expr, source, index, terms);
    }

    expression_ref rewrite_as_join_find(
            relation::intermediate::join& expr,
            relation::scan& source,
            storage::index const& index,
            sequence_view<scan_key_collector::term*> terms) {
        BOOST_ASSERT(terms.size() <= index.keys().size()); // NOLINT

        std::vector<relation::join_find::key> keys {};
        fill_search_key(index, keys, terms);

        binding::factory bindings {};
        auto&& result = work_graph_.emplace<relation::join_find>(
                expr.operator_kind(),
                bindings(index),
                std::move(source.columns()),
                std::move(keys),
                expr.release_condition());

        // reconnect
        BOOST_ASSERT(expr.left().opposite()); // NOLINT
        BOOST_ASSERT(expr.right().opposite()); // NOLINT
        BOOST_ASSERT(expr.output().opposite()); // NOLINT
        auto&& upstream = *expr.left().opposite();
        auto&& downstream = *expr.output().opposite();
        expr.left().disconnect_all();
        expr.right().disconnect_all();
        expr.output().disconnect_all();
        result.left().connect_to(upstream);
        result.output().connect_to(downstream);

        // NOTE: orphaned scan operator will be removed later

        return result.ownership_condition();
    }

    expression_ref rewrite_as_join_scan(
            relation::intermediate::join& expr,
            relation::scan& source,
            storage::index const& index,
            sequence_view<scan_key_collector::term*> terms) {
        BOOST_ASSERT(terms.size() <= index.keys().size()); // NOLINT

        binding::factory bindings {};
        relation::join_scan::endpoint lower {};
        relation::join_scan::endpoint upper {};

        fill_endpoints(index, lower, upper, terms);

        auto&& result = work_graph_.emplace<relation::join_scan>(
                expr.operator_kind(),
                bindings(index),
                std::move(source.columns()),
                std::move(lower),
                std::move(upper),
                expr.release_condition());

        // reconnect
        BOOST_ASSERT(expr.left().opposite()); // NOLINT
        BOOST_ASSERT(expr.right().opposite()); // NOLINT
        BOOST_ASSERT(expr.output().opposite()); // NOLINT
        auto&& upstream = *expr.left().opposite();
        auto&& downstream = *expr.output().opposite();
        expr.left().disconnect_all();
        expr.right().disconnect_all(); // make scan orphaned
        expr.output().disconnect_all();
        result.left().connect_to(upstream);
        result.output().connect_to(downstream);

        // NOTE: orphaned scan/join operator will be removed later

        return result.ownership_condition();
    }

    void merge_filters(expression_ref condition, sequence_view<relation::filter*> filters) {
        std::unique_ptr<scalar::expression> result;
        if (auto&& current = condition.find();
                current
                && current != boolean_expression(true)) {
            result = condition.exchange({});
        }
        for (auto* filter : filters) {
            if (filter->condition() != boolean_expression(true)) {
                if (result) {
                    result = std::make_unique<scalar::binary>(
                            scalar::binary_operator::conditional_and,
                            std::move(result),
                            filter->release_condition());
                } else {
                    result = filter->release_condition();
                }
            }
            // NOTE: orphaned filter operator will be removed later
            filter->input().disconnect_all();
            filter->output().disconnect_all();
        }
        condition = std::move(result);
    }
};

} // namespace

// FIXME: revise

void rewrite_join(
        relation::graph_type& graph,
        analyzer::index_estimator const& index_estimator,
        flow_volume_info const& flow_volume,
        bool allow_join_scan) {
    engine e { index_estimator, flow_volume, allow_join_scan };
    e.process(graph);
}

} // namespace yugawara::analyzer::details
