#pragma once

#include <yugawara/analyzer/index_selection_parameters.h>

#include "estimate_index.h"

namespace yugawara::analyzer::details {

/**
 * @brief the result of comparing two index candidates.
 */
enum class candidate_comparison {
    /// @brief the first candidate is better than the second one.
    better,
    /// @brief the first candidate is worse than the second one.
    worse,
    /// @brief the two candidates are equivalent.
    equivalent,
};

/**
 * @brief how the index access provides the row order requested by the downstream operation.
 */
enum class candidate_order {
    /// @brief does not provide the row order, so that the rows must be sorted later.
    none,
    /// @brief provides the row order, and the scan stops at the requested number of rows.
    early_stop,
    /// @brief provides the row order, but the scan may read the whole range.
    ordered,
};

/**
 * @brief returns whether or not the index access fetches at most one entry.
 * @details This relies on the estimated number of entries, so that the result must not be used for correctness.
 * @param candidate the target candidate
 * @return true if the access is estimated to fetch at most one entry
 * @return false otherwise
 */
[[nodiscard]] bool is_single_row(index_candidate const& candidate) noexcept;

/**
 * @brief compares the estimated costs of the two index candidates.
 * @details This compares the candidates by the following rules:
 *
 *      1. the candidate which fetches at most one entry is better (see is_single_row())
 *      2. the candidate with the lower estimated cost is better:
 *         the estimated cost is the estimated number of entries if both candidates have it,
 *         or the estimated selectivity otherwise,
 *         and then it is multiplied by index_selection_parameters::indirect_access_weight()
 *         if the access is not direct
 *
 *      The candidates may be on different scan operations.
 * @param first the first candidate
 * @param second the second candidate
 * @param parameters the index selection parameters
 * @return the comparison result
 */
[[nodiscard]] candidate_comparison compare_index_candidates(
        index_candidate const& first,
        index_candidate const& second,
        index_selection_parameters const& parameters);

/**
 * @brief compares the estimated costs of the two index candidates, with scaling the costs.
 * @details This is same as compare_index_candidates(index_candidate const&, index_candidate const&, index_selection_parameters const&),
 *      except the estimated costs are divided by the individual scales.
 * @param first the first candidate
 * @param first_scale the scale of the first candidate, must be positive
 * @param second the second candidate
 * @param second_scale the scale of the second candidate, must be positive
 * @param parameters the index selection parameters
 * @return the comparison result
 */
[[nodiscard]] candidate_comparison compare_index_candidates(
        index_candidate const& first,
        double first_scale,
        index_candidate const& second,
        double second_scale,
        index_selection_parameters const& parameters);

/**
 * @brief returns whether or not the candidate is better than the current best one for the same scan operation.
 * @details In addition to compare_index_candidates(), this breaks ties by the following rules,
 *      so that the result does not depend on the order of the candidates:
 *
 *      1. the direct access is better
 *      2. the index with the smaller name is better
 * @param candidate the target candidate
 * @param best the current best candidate
 * @param parameters the index selection parameters
 * @return true if the candidate is better than the current best one
 * @return false otherwise
 */
[[nodiscard]] bool is_better_scan_candidate(
        index_candidate const& candidate,
        index_candidate const& best,
        index_selection_parameters const& parameters);

/**
 * @brief returns whether or not the candidate is better than the current best one for the same scan operation,
 *      whose downstream operation requests the row order.
 * @details This compares the candidates by the following rules:
 *
 *      1. the candidate which fetches at most one entry is better (see is_single_row())
 *      2. the candidate which stops early in the row order (candidate_order::early_stop) is better,
 *         because it reads fewer rows than any other candidates
 *      3. the candidate with the lower upper-bound cost is better:
 *         the upper-bound cost is the estimated cost when the scan does not stop early,
 *         that is, the estimated selectivity multiplied by index_selection_parameters::indirect_access_weight()
 *         if the access is not direct, and then the candidates without the row order (candidate_order::none)
 *         also requires the cost to sort the rows (the sort selectivity multiplied by
 *         index_selection_parameters::sort_cost())
 *      4. the ties are broken as same as is_better_scan_candidate()
 * @param candidate the target candidate
 * @param order how the target candidate provides the row order
 * @param best the current best candidate
 * @param best_order how the current best candidate provides the row order
 * @param sort_selectivity the estimated ratio of rows to sort
 * @param parameters the index selection parameters
 * @return true if the candidate is better than the current best one
 * @return false otherwise
 */
[[nodiscard]] bool is_better_ordered_scan_candidate(
        index_candidate const& candidate,
        candidate_order order,
        index_candidate const& best,
        candidate_order best_order,
        double sort_selectivity,
        index_selection_parameters const& parameters);

} // namespace yugawara::analyzer::details
