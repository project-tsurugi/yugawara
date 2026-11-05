#pragma once

#include <cstddef>

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief places `limit` operators to upstream.
 * @details This moves each `limit` over the upstream operators which have a single input,
 *      and whose path preserves the row order and makes exactly one row from each input row
 *      (see find_path_capabilities()), only if the `limit` does not refer to any columns defined in the operator.
 *      Currently, such operators are only `project`, and `buffer` which can stop with the primary output
 *      (only if the `limit` is in the downstream of the primary output, see `buffer::stop_with_primary()`).
 *      This never moves `limit` over the other operators, like `filter`, `join`, or aggregations.
 *      Currently, this only moves `limit` operators which have the row count and do not have any group keys.
 *
 *      If the `limit` reaches another upstream `limit`, this merges its row count into the upstream one,
 *      only if both do not have any group keys nor the start position,
 *      and the sort keys of the downstream `limit` are a prefix of the upstream ones (including empty).
 *      The upstream `limit` keeps its sort keys, and has the smaller row count of them.
 *      This never removes the downstream `limit`, and it will be removed later if it is redundant
 *      (see collect_exchange_steps()).
 *
 *      If the `limit` reaches an operator whose input path makes one or more rows from each input row
 *      (see find_path_capabilities()), this places at most one copy of the `limit` (partial limit) into
 *      the farthest upstream of the input, and keeps the original `limit` in the downstream.
 *      Currently, such paths are only the left input of left outer joins, and the input of outer apply.
 *      The partial limit is placed over the upstream operators in the same way as the original one,
 *      and also over the paths above, because the original `limit` is kept in the downstream.
 *      This only places the partial limit if the `limit` does not have the start position,
 *      its row count is an immediate value which is less than or equal to `max_partial_limit_count`,
 *      and its sort keys only refer the columns of the input.
 *      If the partial limit reaches another upstream `limit`, this merges its row count into the upstream one
 *      instead of placing the partial limit, even if the row count is greater than `max_partial_limit_count`,
 *      because it never increases the number of operators.
 *
 *      This processes the `limit` operators from downstream to upstream,
 *      so that the upstream `limit` operators are moved with their row count shrunk by the downstream ones.
 * @attention The graph must not contain the `filter` operators whose condition is just `TRUE`
 *      (see remove_redundant_conditions()), because this treats them as the regular `filter` operators,
 *      and then they stop moving `limit` operators.
 * @note This may reduce the number of rows that `project` evaluates,
 *      so that the errors in evaluating the dropped rows will not be raised.
 * @param graph the target graph
 * @param max_partial_limit_count the maximum row count of the partial limits,
 *      or `0` to disable them (including merging into the upstream `limit` over the paths above)
 * @see collect_exchange_steps()
 */
void push_down_limits(::takatori::relation::graph_type& graph, std::size_t max_partial_limit_count);

} // namespace yugawara::analyzer::details
