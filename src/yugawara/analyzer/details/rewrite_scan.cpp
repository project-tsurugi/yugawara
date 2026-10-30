#include "rewrite_scan.h"

#include <glog/logging.h>

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
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

    /// @brief the number of conjunctive terms in the `filter` operators between the `scan` and the `limit`.
    std::size_t conditions;
};

[[nodiscard]] std::size_t count_conditions(relation::scan& expr) {
    std::size_t result = 0;
    optional_ptr<output_port> current { expr.output() };
    while (current && current->opposite()) {
        auto&& next = current->opposite()->owner();
        if (next.kind() == relation::filter::tag) {
            auto&& filter = unsafe_downcast<relation::filter>(next);
            decompose_predicate(filter.ownership_condition(), [&](auto&& term) {
                // NOTE: the constant TRUE will be removed later (see remove_redundant_conditions())
                if (*term != boolean_expression(true)) {
                    ++result;
                }
            });
            current = optional_ptr<output_port> { filter.output() };
        } else if (next.kind() == relation::project::tag) {
            current = optional_ptr<output_port> { unsafe_downcast<relation::project>(next).output() };
        } else {
            break;
        }
    }
    return result;
}

[[nodiscard]] std::optional<order_request> find_order_request(relation::scan& expr) {
    auto destination = find_ordered_destination(expr);
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
            count_conditions(expr),
    };
}

/**
 * @brief returns the number of conjunctive terms which the candidate consumes as its search keys.
 * @param candidate the target candidate
 * @return the number of consumed terms
 */
[[nodiscard]] std::size_t count_consumed_conditions(index_candidate const& candidate) {
    std::size_t result = 0;
    for (auto const* term : candidate.terms) {
        if (term->equivalent()) {
            ++result;
            continue;
        }
        if (term->lower_factor()) {
            ++result;
        }
        if (term->upper_factor()) {
            ++result;
        }
    }
    return result;
}

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
        std::vector<index_candidate> candidates {};
        storages->each_table_index(
                table,
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    auto current = estimate_index(collector, *entry, index_estimator_);
                    VLOG(50) << "index " << entry->simple_name() << ": " << current.result;
                    candidates.emplace_back(std::move(current));
                });
        if (candidates.empty()) {
            return;
        }
        std::size_t selected = request
                ? select_ordered(candidates, *request)
                : select(candidates);
        apply_index_to_scan(expr, candidates[selected], work_graph_);
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

    [[nodiscard]] std::size_t select_ordered(
            std::vector<index_candidate> const& candidates,
            order_request const& request) const {
        std::vector<candidate_order> orders {};
        orders.reserve(candidates.size());
        double sort_selectivity = 1.0;
        for (auto&& candidate : candidates) {
            orders.emplace_back(decide_order(candidate, request));
            // NOTE: the rows to sort are at most the rows of the most selective candidate
            sort_selectivity = std::min(sort_selectivity, candidate.result.selectivity());
        }
        std::size_t saved = 0;
        for (std::size_t i = 1, n = candidates.size(); i < n; ++i) {
            if (is_better_ordered_scan_candidate(
                    candidates[i],
                    orders[i],
                    candidates[saved],
                    orders[saved],
                    sort_selectivity,
                    parameters_)) {
                saved = i;
            }
        }
        return saved;
    }

    [[nodiscard]] candidate_order decide_order(index_candidate const& candidate, order_request const& request) const {
        // NOTE: find operations do not provide any row order
        if (candidate.kind == index_candidate::access_kind::find || !candidate.scannable) {
            return candidate_order::none;
        }
        auto fixed = fixed_columns(candidate);
        if (!find_scan_direction(candidate.index, request.sort_keys, fixed)) {
            return candidate_order::none;
        }
        // NOTE: the scan can stop at the row count, only if the index consumes all conditions
        bool residual = count_consumed_conditions(candidate) < request.conditions;
        if (request.count && !residual && features_.contains(runtime_feature::scan_limit)) {
            return candidate_order::early_stop;
        }
        // NOTE: the limit with the row count becomes a forward exchange with ordering,
        // and the limit without the row count will be removed (see remove_redundant_limits())
        if (features_.contains(runtime_feature::ordered_scan)
                && (!request.counted || features_.contains(runtime_feature::ordered_forward))) {
            return candidate_order::ordered;
        }
        return candidate_order::none;
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
