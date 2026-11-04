#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/runtime_feature.h>
#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/analyzer/index_selection_parameters.h>

namespace yugawara::analyzer::details {

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
 * @param features the available runtime features, which decide whether the `scan` can provide the row order
 * @see absorb_limits()
 */
void rewrite_scan(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator,
        index_selection_parameters const& parameters,
        runtime_feature_set const& features);

} // namespace yugawara::analyzer::details
