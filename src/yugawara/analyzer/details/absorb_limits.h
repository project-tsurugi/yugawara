#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/runtime_feature.h>
#include <yugawara/analyzer/index_estimator.h>

namespace yugawara::analyzer::details {

/**
 * @brief absorbs the row count and the row order of `limit` operators into the upstream `scan` operators.
 * @details This only considers `limit` operators which have the immediate row count without any group keys or
 *      the start position, and their source `scan` operators (see find_ordered_source()).
 *
 *      If the `limit` directly connects to the `scan`, this sets the row count of the `limit` to the `scan`
 *      (requires runtime_feature::scan_limit).
 *      Otherwise, there are some `filter` or `project` operators between them, and this does not set the row count,
 *      because they may reduce the number of rows.
 *
 *      If the `limit` has sort keys, this also sets the direction to the `scan`,
 *      only if reading the index of the `scan` in the direction provides the rows in the order of the sort keys.
 *      If the `scan` already has a direction, this absorbs nothing unless the direction is same as the above.
 *      If the `limit` does not directly connect to the `scan`, this sets only the direction, and requires
 *      runtime_feature::ordered_scan and runtime_feature::ordered_forward, because the direction makes sense only if
 *      the `limit` becomes a forward exchange with ordering.
 *
 *      Before the above, if the index of the `scan` does not provide the rows in the order of the sort keys,
 *      this may select another index which provides them, only if the `scan` does not have any range nor direction.
 *      That is, the scan conditions take precedence over the row order of `limit` operators.
 *
 *      The `limit` operators are retained, so that the downstream operators are not changed.
 * @param graph the target graph
 * @param index_estimator the index cost estimator, to select indices for the row order
 * @param features the available runtime features
 * @see find_ordered_source()
 * @see find_scan_direction()
 */
void absorb_limits(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator,
        runtime_feature_set const& features);

} // namespace yugawara::analyzer::details
