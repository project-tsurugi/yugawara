#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/analyzer/index_selection_parameters.h>

#include "path_capability.h"

namespace yugawara::analyzer::details {

/**
 * @brief the capabilities of `scan` operations, which rewrite_scan() relies on.
 */
struct scan_capability_options {
    /// @brief whether or not the `scan` can read the index backward.
    bool backward { false };

    /// @brief whether or not the `scan` can stop at the row count.
    bool row_limit { false };

    /// @brief whether or not the `scan` can provide the row order.
    bool ordered_output { false };

    /// @brief whether or not the `limit` with the row count can use the row order provided by the `scan`.
    bool ordered_limit { false };
};

/**
 * @brief rewrites `scan` operations into `scan` / `find` with suitable indices.
 * @details This only rewrites `scan` without any scan conditions, row slices nor directions,
 *      and will retain `scan` with bounds or `find`.
 *      This never rewrite `join_relation` into `join_{scan,find}`.
 *
 *      If the downstream `limit` of the `scan` requests the row order (see find_ordered_destination()),
 *      this also considers the indices which provide the row order (see is_better_ordered_scan_candidate()),
 *      even if the `scan` does not have any conditions.
 *      If the selected index provides the requested row order, this also sets the direction to the `scan`,
 *      and the row count of the `limit` if the `scan` can stop at it, even if the row count is too large to
 *      contribute to the index selection (see index_selection_parameters::max_early_stop_limit_count()).
 *      The row counts of the `limit` operators without sort keys are set by absorb_limits() later.
 * @attention The graph must not contain the `filter` operators whose condition is just `TRUE`
 *      except ones directly following the `scan` operators (see remove_redundant_conditions()),
 *      because this treats them as the regular `filter` operators in the path to the downstream `limit`,
 *      and then it may not consider the indices which can stop reading rows early.
 * @param graph the target graph
 * @param index_estimator the index estimator
 * @param parameters the parameters to choose an index from the estimations
 * @param scan_capabilities the capabilities of the `scan`, which decide whether it can provide the row order
 * @param capability_options the options to find the path capabilities (see find_path_capabilities())
 * @see absorb_limits()
 */
void rewrite_scan(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator,
        index_selection_parameters const& parameters,
        scan_capability_options const& scan_capabilities,
        path_capability_options const& capability_options);

} // namespace yugawara::analyzer::details
