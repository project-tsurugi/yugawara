#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/runtime_feature.h>

namespace yugawara::analyzer::details {

/**
 * @brief absorbs the row count of `limit` operators into the upstream `scan` operators.
 * @details This only considers `limit` operators without any sort keys, group keys nor the start position,
 *      which have the immediate row count.
 *      The `limit` operators with sort keys are handled by rewrite_scan(), because their row counts
 *      are only available if the `scan` provides the row order.
 *
 *      This walks from the `limit` to upstream, only while each row of the upstream becomes one or more
 *      input rows of the `limit` (that is, the path has `at_least_one`, see find_path_capabilities()).
 *      If it reaches a `scan`, this sets the smaller one of the row counts of the `limit` and the `scan`
 *      to the `scan` (requires runtime_feature::scan_limit).
 *      The `scan` without the row count is regarded as the one with the infinite row count,
 *      and this does nothing if the `scan` has the variable row count.
 *      This never changes the index nor the direction of the `scan`.
 *
 *      If the walk reaches an operator which has two or more inputs, this walks each input
 *      which has `at_least_one` (e.g. the left input of left outer joins, or each input of union all),
 *      and then absorbs the row count into each `scan` independently.
 *
 *      The `limit` operators are retained, so that the downstream operators are not changed.
 *      The `limit` may be removed later only if the path also has `at_most_one` (see collect_exchange_steps()).
 * @note This does not absorb the row count if a `filter` remains between the `scan` and the `limit`,
 *      for example, if its condition is not consumed by the `scan`, or if it is just `TRUE`
 *      (see remove_redundant_conditions()).
 *      The result is still correct, but the `scan` may read more rows.
 * @param graph the target graph
 * @param features the available runtime features
 * @see find_path_capabilities()
 * @see push_down_limits()
 * @see rewrite_scan()
 */
void absorb_limits(
        ::takatori::relation::graph_type& graph,
        runtime_feature_set const& features);

} // namespace yugawara::analyzer::details
