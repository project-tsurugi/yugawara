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
 *      Note that this only selects the index, and absorb_limits() sets the direction and the row count to the `scan`
 *      later.
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
