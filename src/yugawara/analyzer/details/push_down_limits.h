#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief places `limit` operators to upstream.
 * @details This moves each `limit` over the upstream `project` operators,
 *      only if the `limit` does not refer to any columns defined in the `project`.
 *      This never moves `limit` over the other operators, like `filter`, `join`, or aggregations.
 *      Currently, this only moves `limit` operators which have the row count and do not have any group keys.
 * @note This may reduce the number of rows that `project` evaluates,
 *      so that the errors in evaluating the dropped rows will not be raised.
 * @param graph the target graph
 */
void push_down_limits(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
