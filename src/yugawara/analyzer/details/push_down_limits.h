#pragma once

#include <cstddef>

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief places `limit` operators to upstream.
 * @details This moves each `limit` over the upstream operators which have a single input,
 *      and whose path preserves the row order and makes exactly one row from each input row
 *      (see find_path_capabilities()), only if the `limit` does not refer to any columns defined in the operator.
 *      Currently, such operators are only `project`.
 *      This never moves `limit` over the other operators, like `filter`, `join`, or aggregations.
 *      Currently, this only moves `limit` operators which have the row count and do not have any group keys.
 *
 *      If the `limit` reaches another upstream `limit`, this merges them into the upstream one,
 *      only if both do not have any group keys nor the start position,
 *      and the sort keys of the downstream `limit` are a prefix of the upstream ones (including empty).
 *      The merged `limit` has the sort keys of the upstream one, and the smaller row count of them.
 *
 *      If the `limit` reaches an operator whose input path makes one or more rows from each input row
 *      (see find_path_capabilities()), this places a copy of the `limit` (partial limit) into the input,
 *      and keeps the original `limit` in the downstream.
 *      Currently, such paths are only the left input of left outer joins.
 *      This only places the partial limit if the `limit` does not have the start position,
 *      its row count is an immediate value which is less than or equal to `max_partial_limit_count`,
 *      and its sort keys only refer the columns of the input.
 *      The partial limit is moved over the upstream operators in the same way,
 *      but it is moved instead of copied if it reaches the paths above,
 *      because the original `limit` is kept in the downstream.
 * @note This may reduce the number of rows that `project` evaluates,
 *      so that the errors in evaluating the dropped rows will not be raised.
 * @param graph the target graph
 * @param max_partial_limit_count the maximum row count of the partial limits, or `0` to disable them
 */
void push_down_limits(::takatori::relation::graph_type& graph, std::size_t max_partial_limit_count);

} // namespace yugawara::analyzer::details
