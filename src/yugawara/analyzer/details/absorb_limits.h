#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/runtime_feature.h>

namespace yugawara::analyzer::details {

/**
 * @brief absorbs the row count and the row order of `limit` operators into the upstream `scan` operators.
 * @details This only considers `limit` operators without any group keys nor the start position,
 *      which have the immediate row count or only the sort keys, and their source `scan` operators
 *      (see find_ordered_source()).
 *      This does not select indices: the index of the `scan` has already been selected by rewrite_scan(),
 *      with considering the row order of the `limit`.
 *
 *      If the `limit` has the row count and directly connects to the `scan`,
 *      this sets the row count of the `limit` to the `scan` (requires runtime_feature::scan_limit).
 *      Otherwise, there are some `filter` or `project` operators between them, and this does not set the row count,
 *      because they may reduce the number of rows.
 *
 *      If the `limit` has sort keys, this also sets the direction to the `scan`,
 *      only if reading the index of the `scan` in the direction provides the rows in the order of the sort keys.
 *      If the `scan` already has a direction, this absorbs nothing unless the direction is same as the above.
 *      The backward direction requires runtime_feature::backward_scan.
 *      If this does not set the row count, this sets only the direction, and requires runtime_feature::ordered_scan:
 *      the `limit` with the row count becomes a forward exchange with ordering
 *      (also requires runtime_feature::ordered_forward),
 *      and the `limit` without the row count will be removed (see remove_redundant_limits()).
 *
 *      The `limit` operators are retained, so that the downstream operators are not changed.
 * @param graph the target graph
 * @param features the available runtime features
 * @see find_ordered_source()
 * @see find_scan_direction()
 * @see rewrite_scan()
 */
void absorb_limits(
        ::takatori::relation::graph_type& graph,
        runtime_feature_set const& features);

} // namespace yugawara::analyzer::details
