#include "rewrite_scan.h"

#include <glog/logging.h>

#include <algorithm>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <takatori/relation/filter.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/scalar/expression.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/extract.h>

#include <yugawara/storage/index.h>
#include <yugawara/storage/provider.h>

#include "apply_index_to_scan.h"
#include "boolean_constants.h"
#include "compare_index_candidates.h"
#include "decompose_predicate.h"
#include "estimate_index.h"
#include "find_scan_direction.h"
#include "ordered_relation.h"
#include "scan_key_collector.h"
#include "remove_orphaned_elements.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

using output_port = relation::expression::output_port_type;

namespace {

/**
 * @brief the row order requested by the downstream `limit` of a `scan`.
 */
struct order_request {
    /// @brief the sort keys, as the columns of the scan.
    std::vector<storage::details::index_key_element> sort_keys;

    /// @brief whether or not the `limit` has the row count.
    bool counted;

    /// @brief the immediate row count of the `limit`, or empty if it is absent or not immediate.
    std::optional<std::size_t> count;

    /// @brief the `filter` operators which directly follow the scan (see collect_succeeding_filters()).
    std::vector<relation::filter*> filters;

    /// @brief the capabilities of the path from the last `filters` (or the scan) to the `limit`.
    path_capability_set capabilities;
};

/**
 * @brief returns the `filter` operators which directly follow the scan.
 * @details The scan can consume the conditions only in these filters (see scan_key_collector::scope::scan).
 * @param expr the target scan
 * @return the succeeding filters, in order from the scan
 */
[[nodiscard]] std::vector<relation::filter*> collect_succeeding_filters(relation::scan& expr) {
    std::vector<relation::filter*> results {};
    optional_ptr<output_port> current { expr.output() };
    while (current && current->opposite()) {
        auto&& next = current->opposite()->owner();
        if (next.kind() != relation::filter::tag) {
            break;
        }
        auto&& filter = unsafe_downcast<relation::filter>(next);
        results.emplace_back(std::addressof(filter));
        current = optional_ptr<output_port> { filter.output() };
    }
    return results;
}

[[nodiscard]] std::optional<order_request> find_order_request(relation::scan& expr) {
    auto filters = collect_succeeding_filters(expr);
    auto path = filters.empty()
            ? find_ordered_destination(expr)
            : find_ordered_destination(filters.back()->output());
    auto&& destination = path.end;
    if (!destination || destination->kind() != relation::intermediate::limit::tag) {
        return {};
    }
    auto&& limit = unsafe_downcast<relation::intermediate::limit>(*destination);
    if (limit.sort_keys().empty() || !limit.group_keys().empty()) {
        return {};
    }
    auto&& slice = limit.row_slice();
    // FIXME: currently we do not consider the limit with the effective start position (other than immediate `0`)
    if (auto&& start = slice.start(); !start.is_absent()) {
        if (auto position = start.immediate_if(); !position || *position != 0) {
            return {};
        }
    }
    auto sort_keys = resolve_sort_keys(expr, limit);
    if (!sort_keys) {
        return {};
    }
    std::optional<std::size_t> count {};
    // NOTE: the limit with the variable count may not stop early (it can be NULL)
    if (auto value = slice.count().immediate_if()) {
        count.emplace(*value);
    }
    return order_request {
            std::move(*sort_keys),
            !slice.count().is_absent(),
            count,
            std::move(filters),
            path.capabilities,
    };
}

/**
 * @brief returns whether or not the candidate consumes all conditions in the given filters.
 * @details This checks the conjunctive terms in the `filter` operators, and returns `false` if any of them is not
 *      consumed by the candidate as its search keys.
 *      The terms are compared by their identity, so that this returns `false` for the terms which the
 *      candidate does not know.
 *      If this returns `true`, the filters will be removed after applying the candidate
 *      (see remove_redundant_conditions()).
 * @param filters the filters which directly follow the scan (see collect_succeeding_filters())
 * @param candidate the candidate to apply
 * @return true if the candidate consumes all conditions
 * @return false if any conditions remain
 */
[[nodiscard]] bool consumes_all_conditions(
        std::vector<relation::filter*> const& filters,
        index_candidate const& candidate) {
    std::vector<::takatori::scalar::expression const*> consumed {};
    for (auto const* term : candidate.terms) {
        for (auto&& element : { term->equivalent_term(), term->lower_term(), term->upper_term() }) {
            if (element) {
                consumed.emplace_back(element.get());
            }
        }
    }
    bool result = true;
    for (auto* filter : filters) {
        // NOTE: this never modifies the condition, but the overload with ownership also passes
        // the condition itself if it is not a conjunction
        decompose_predicate(filter->ownership_condition(), [&](auto&& term) {
            // NOTE: the constant TRUE will be removed later (see remove_redundant_conditions())
            if (*term == boolean_expression(true)) {
                return;
            }
            if (std::find(consumed.begin(), consumed.end(), std::addressof(*term)) == consumed.end()) {
                result = false;
            }
        });
        if (!result) {
            break;
        }
    }
    return result;
}

/**
 * @brief returns whether or not each row of the scan becomes exactly one input row of the requesting `limit`,
 *      after applying the candidate to the scan.
 * @param request the order request
 * @param candidate the candidate to apply
 * @return true if each row of the scan becomes exactly one input row of the `limit`
 * @return false otherwise
 */
[[nodiscard]] bool keeps_rows(order_request const& request, index_candidate const& candidate) {
    auto capabilities = request.capabilities;
    // NOTE: the succeeding filters remain if the candidate does not consume all of their conditions
    if (!consumes_all_conditions(request.filters, candidate)) {
        capabilities.erase(path_capability::at_least_one);
    }
    return capabilities.contains(path_capability::at_least_one)
            && capabilities.contains(path_capability::at_most_one);
}

/**
 * @brief how an index candidate provides the row order requested by the downstream `limit`.
 */
struct order_decision {
    /// @brief how the candidate provides the row order.
    candidate_order order { candidate_order::none };

    /// @brief the scan direction to provide the row order, or empty if the candidate does not provide it.
    std::optional<relation::scan_direction> direction {};
};

class engine {
public:
    explicit engine(
            index_estimator const& index_estimator,
            index_selection_parameters const& parameters,
            runtime_feature_set const& features) :
        index_estimator_ { index_estimator },
        parameters_ { parameters },
        features_ { features }
    {}

    void process(relation::graph_type& graph) {
        work_graph_.clear();
        for (auto&& expr : graph) {
            if (expr.kind() == relation::scan::tag) {
                auto&& scan = unsafe_downcast<relation::scan>(expr);
                if (!scan.row_slice() && !scan.lower() && !scan.upper() && !scan.direction()) {
                    process(scan);
                }
            }
        }
        install(graph);
    }

private:
    index_estimator const& index_estimator_;
    index_selection_parameters const& parameters_;
    runtime_feature_set const& features_;

    relation::graph_type work_graph_ {};

    void process(relation::scan& expr) {
        // NOTE: the collector and the candidate are only valid while processing the current scan
        scan_key_collector collector {};
        // NOTE: this may return false if the scan has no conditions, but the collector is still available
        bool has_terms = collector(expr, scan_key_collector::scope::scan);
        auto request = find_order_request(expr);
        if (!has_terms && !request) {
            return;
        }
        auto&& table = binding::extract<storage::index>(expr.source()).table();
        auto storages = table.owner();
        if (!storages) {
            // there is no index information
            return;
        }
        // NOTE: each_table_index() may hold a lock of the provider, so that we only collect the indices in it
        std::vector<std::shared_ptr<storage::index const>> indices {};
        storages->each_table_index(
                table,
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    indices.emplace_back(entry);
                });
        if (indices.empty()) {
            return;
        }
        std::vector<index_candidate> candidates {};
        candidates.reserve(indices.size());
        for (auto&& entry : indices) {
            auto current = estimate_index(collector, *entry, index_estimator_);
            VLOG(50) << "index " << entry->simple_name() << ": " << current.result;
            candidates.emplace_back(std::move(current));
        }
        if (!request) {
            apply_index_to_scan(expr, candidates[select(candidates)], work_graph_);
            return;
        }
        auto [selected, decision] = select_ordered(candidates, *request);
        apply_index_to_scan(expr, candidates[selected], work_graph_);
        // NOTE: the candidate which provides the row order is always a scan (see decide_order()),
        // so that the target scan is still alive here
        apply_order(expr, decision, *request);
    }

    static void apply_order(relation::scan& expr, order_decision const& decision, order_request const& request) {
        if (decision.order == candidate_order::none) {
            return;
        }
        expr.direction() = decision.direction;
        if (decision.order == candidate_order::early_stop) {
            expr.row_slice() = relation::scan::row_slice_type { {}, *request.count };
        }
    }

    [[nodiscard]] std::size_t select(std::vector<index_candidate> const& candidates) const {
        std::size_t saved = 0;
        for (std::size_t i = 1, n = candidates.size(); i < n; ++i) {
            if (is_better_scan_candidate(candidates[i], candidates[saved], parameters_)) {
                saved = i;
            }
        }
        return saved;
    }

    [[nodiscard]] std::pair<std::size_t, order_decision> select_ordered(
            std::vector<index_candidate> const& candidates,
            order_request const& request) const {
        std::vector<order_decision> decisions {};
        decisions.reserve(candidates.size());
        double sort_selectivity = 1.0;
        for (auto&& candidate : candidates) {
            decisions.emplace_back(decide_order(candidate, request));
            // NOTE: the rows to sort are at most the rows of the most selective candidate
            sort_selectivity = std::min(sort_selectivity, candidate.result.selectivity());
        }
        std::size_t saved = 0;
        for (std::size_t i = 1, n = candidates.size(); i < n; ++i) {
            if (is_better_ordered_scan_candidate(
                    candidates[i],
                    decisions[i].order,
                    candidates[saved],
                    decisions[saved].order,
                    sort_selectivity,
                    parameters_)) {
                saved = i;
            }
        }
        return { saved, decisions[saved] };
    }

    [[nodiscard]] order_decision decide_order(
            index_candidate const& candidate,
            order_request const& request) const {
        // NOTE: find operations do not provide any row order
        if (candidate.kind == index_candidate::access_kind::find || !candidate.scannable) {
            return {};
        }
        auto fixed = fixed_columns(candidate);
        auto direction = find_scan_direction(candidate.index, request.sort_keys, fixed);
        if (!direction) {
            return {};
        }
        if (*direction == relation::scan_direction::backward && !features_.contains(runtime_feature::backward_scan)) {
            return {};
        }
        // NOTE: the scan can stop at the row count, only if each row of the scan reaches the limit;
        // a limit with a large row count is regarded as same as the limit without the row count,
        // because it rarely stops the scan early (see index_selection_parameters::max_early_stop_limit_count())
        if (request.count
                && *request.count <= parameters_.max_early_stop_limit_count()
                && features_.contains(runtime_feature::scan_limit)
                && keeps_rows(request, candidate)) {
            return { candidate_order::early_stop, direction };
        }
        // NOTE: the limit with the row count becomes a forward exchange with ordering,
        // and the limit without the row count will be removed (see remove_redundant_limits())
        if (features_.contains(runtime_feature::ordered_scan)
                && (!request.counted || features_.contains(runtime_feature::ordered_forward))) {
            return { candidate_order::ordered, direction };
        }
        return {};
    }

    void install(relation::graph_type& graph) {
        relation::merge_into(std::move(work_graph_), graph);
        work_graph_.clear();

        remove_orphaned_elements(graph);
    }
};

} // namespace

void rewrite_scan(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator,
        index_selection_parameters const& parameters,
        runtime_feature_set const& features) {
    engine e { index_estimator, parameters, features };
    e.process(graph);
}

} // namespace yugawara::analyzer::details
