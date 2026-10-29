#include "rewrite_join.h"

#include <functional>
#include <optional>
#include <utility>
#include <vector>

#include <takatori/scalar/binary.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/relation/filter.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/provider.h>

#include "boolean_constants.h"
#include "estimate_index.h"
#include "scan_key_collector.h"
#include "remove_orphaned_elements.h"
#include "rewrite_criteria.h"

namespace yugawara::analyzer::details {

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using attribute = details::index_estimator_result_attribute;

using ::takatori::relation::join_kind;
using ::takatori::relation::join_kind_set;
using ::takatori::util::ownership_reference;
using ::takatori::util::optional_ptr;
using ::takatori::util::sequence_view;
using ::takatori::util::unsafe_downcast;

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
                // NOTE: the join with end-points is already rewritten (by collect_join_keys) or built by other front-ends
                if (!join.lower() && !join.upper()) {
                    process(join);
                }
            }
        }
        install(graph);
    }

private:
    /// @brief the join input to rewrite as an index access.
    enum class join_side {
        /// @brief the left input, which requires to swap the join inputs.
        left,
        /// @brief the right input.
        right,
    };

    /**
     * @brief an index candidate for the current join.
     * @details The terms point to the elements in the scan_key_collector of the current join,
     *      so that this must not outlive it.
     */
    struct candidate {
        std::reference_wrapper<relation::scan> scan;
        join_side side;
        index_candidate access;
        std::vector<relation::filter*> filters;
    };

    index_estimator const& index_estimator_;
    flow_volume_info const& flow_volume_;
    bool allow_join_scan_;

    relation::graph_type work_graph_ {};

    void process(relation::intermediate::join& expr) {
        // NOTE: the collectors and the candidate are only valid while processing the current join
        scan_key_collector right_collector {};
        scan_key_collector left_collector {};
        std::optional<candidate> saved {};
        // NOTE: we estimate the both inputs, and then rewrite only one of them (see is_better())
        process_right(expr, right_collector, saved);
        process_left(expr, left_collector, saved);
        if (saved) {
            auto condition = apply_result(expr, *saved);
            merge_filters(std::move(condition), saved->filters);
        }
    }

    void install(relation::graph_type& graph) {
        relation::merge_into(std::move(work_graph_), graph);
        work_graph_.clear();

        remove_orphaned_elements(graph);
    }

    void process_left(
            relation::intermediate::join& expr,
            scan_key_collector& collector,
            std::optional<candidate>& saved) const {
        // rewriting the left input requires to swap the join inputs (see apply_result()),
        // and only the inner join keeps its semantics after that
        static constexpr join_kind_set allow {
                join_kind::inner,
        };
        if (!allow.contains(expr.operator_kind())) {
            return;
        }
        process_input(expr, join_side::left, false, collector, saved);
    }

    void process_right(
            relation::intermediate::join& expr,
            scan_key_collector& collector,
            std::optional<candidate>& saved) const {
        // the join kinds that join_find and join_scan can represent with the right input as the index side
        static constexpr join_kind_set allow {
                join_kind::inner,
                join_kind::semi,
                join_kind::anti,
                join_kind::left_outer,
                join_kind::left_outer_at_most_one,
        };
        static constexpr join_kind_set direct {
                join_kind::anti,
                join_kind::left_outer,
                join_kind::left_outer_at_most_one,
        };
        if (!allow.contains(expr.operator_kind())) {
            return;
        }
        process_input(expr, join_side::right, direct.contains(expr.operator_kind()), collector, saved);
    }

    /**
     * @brief estimates the indices for the scan on the given join input.
     * @param expr the target join
     * @param side the join input to estimate
     * @param direct whether or not the scan must directly connect to the join, without any filters
     * @param collector the collector for the join input
     * @param saved the best candidate so far, which will be replaced if better one is found
     */
    void process_input(
            relation::intermediate::join& expr,
            join_side side,
            bool direct,
            scan_key_collector& collector,
            std::optional<candidate>& saved) const {
        std::vector<relation::filter*> filters {};
        auto scan = find_scan(input(expr, side), direct, filters);
        if (!scan || !collector(*scan, scan_key_collector::scope::join)) {
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
                    auto current = estimate(*scan, *entry, collector, side, filters);
                    // FIXME: use more statistics of individual join inputs
                    if (current && is_better(expr, saved, *current)) {
                        saved = std::move(current);
                    }
                });
    }

    static optional_ptr<relation::scan> find_scan(
            relation::expression::input_port_type& input,
            bool direct,
            std::vector<relation::filter*>& filters) {
        auto current = input.opposite();
        while (true) {
            BOOST_ASSERT(current); // NOLINT
            auto&& expr = current->owner();
            using kind = relation::expression_kind;
            switch (expr.kind()) {
                case kind::scan: {
                    auto&& result = unsafe_downcast<relation::scan>(expr);
                    // the scan with end-points is already rewritten,
                    // and the scan with row_slice cannot be the index side,
                    // because its row_slice will be applied for each left row instead of the whole scan
                    if (result.lower() || result.upper() || result.row_slice()) {
                        return {};
                    }
                    return result;
                }

                case kind::filter: {
                    if (direct) {
                        return {};
                    }
                    auto&& filter = unsafe_downcast<relation::filter>(expr);
                    filters.emplace_back(std::addressof(filter));
                    current = filter.input().opposite();
                    break; // continue to upstream
                }

                default:
                    return {};
            }
        }
    }

    std::optional<candidate> estimate(
            relation::scan& scan,
            storage::index const& index,
            scan_key_collector& collector,
            join_side side,
            sequence_view<relation::filter* const> filters) const {
        auto access = estimate_index(collector, index, index_estimator_);
        if (access.terms.empty()) {
            // index based join must have at least one term
            // NOTE: the terms may only consist of constants (e.g. "R.x = 1"),
            // then the index access is just same for every left row, but it is still valid
            return {};
        }
        auto&& attributes = access.result.attributes();
        if (!attributes.contains(attribute::find) &&
                !(allow_join_scan_ && attributes.contains(attribute::range_scan))) {
            // no accepted methods
            return {};
        }
        return candidate {
                scan,
                side,
                std::move(access),
                { filters.begin(), filters.end() },
        };
    }

    static relation::expression::input_port_type& input(relation::intermediate::join& expr, join_side side) {
        return side == join_side::left ? expr.left() : expr.right();
    }

    static relation::expression::input_port_type const& input(relation::intermediate::join const& expr, join_side side) {
        return side == join_side::left ? expr.left() : expr.right();
    }

    bool is_better(
            relation::intermediate::join const& expr,
            std::optional<candidate> const& saved,
            candidate const& current) const {
        if (!saved) {
            return true;
        }
        auto&& saved_result = saved->access.result;
        auto&& result = current.access.result;
        if (!saved_result.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::single_row)) {
            return true;
        }
        auto saved_score = saved_result.score();
        auto result_score = result.score();
        // NOTE: currently flow_volume_info is always empty, so that this never weights the scores
        auto saved_vol = flow_volume_.find(input(expr, saved->side));
        auto result_vol = flow_volume_.find(input(expr, current.side));
        if (saved_vol && result_vol) {
            saved_score *= saved_vol->row_count * saved_vol->column_size;
            result_score *= result_vol->row_count * result_vol->column_size;
        }
        if (saved_score < result_score) {
            return true;
        }
        if (result_score < saved_score) {
            return false;
        }
        // if the scores are same, we prefer the right input, which does not swap the join inputs
        return saved->side == join_side::left && current.side == join_side::right;
    }

    expression_ref apply_result(relation::intermediate::join& expr, candidate& saved) {
        if (saved.side == join_side::left) {
            swap_upstream(expr.left(), expr.right());
        }
        auto&& source = saved.scan.get();
        auto&& access = saved.access;
        auto&& index = access.index.get();
        if (access.result.attributes().contains(attribute::find)) {
            return rewrite_as_join_find(expr, source, index, access.terms);
        }
        return rewrite_as_join_scan(expr, source, index, access.terms);
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

    /*
     * NOTE: the terms used as the search keys were already removed from the join condition and the filters,
     * and they were replaced with TRUE (see search_key_term::purge_*()).
     * This merges the rest conditions of the filters into the join condition, and then makes the filters orphaned.
     * The orphaned filters (and the orphaned scan) will be removed in install().
     */
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
