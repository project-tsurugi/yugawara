#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief absorbs the row count of `limit` operators into the adjacent upstream `scan` operators.
 * @details This sets the row count of `limit` to the upstream `scan`, only if the `scan` directly connects to the `limit`.
 *      If the `limit` has sort keys, this also sets the direction to the `scan`,
 *      only if reading the index of the `scan` in the direction provides the rows in the order of the sort keys.
 *      If the `scan` already has a direction, this absorbs the row count only if the direction is same as the above.
 *      The `limit` operators are retained, so that the downstream operators are not changed.
 *      Currently, this only absorbs `limit` operators which have the immediate row count without any group keys or
 *      the start position, into `scan` operators which do not have the row count.
 * @param graph the target graph
 * @see find_scan_direction()
 */
void absorb_limits(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
