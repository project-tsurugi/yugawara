#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/runtime_feature.h>

namespace yugawara::analyzer::details {

/**
 * @brief absorbs the row count of `limit` operators into the upstream `scan` operators.
 * @details This only considers `limit` operators without any group keys nor the start position,
 *      which have the immediate row count, and their source `scan` operators (see find_ordered_source()).
 *      This never changes the index nor the direction of the `scan`:
 *      rewrite_scan() has already selected them, and has set the row count if it contributes to the selection.
 *      This sets the rest of the row counts, which do not affect the index selection.
 *
 *      If each row of the `scan` becomes exactly one input row of the `limit`
 *      (that is, the path between them has both `at_least_one` and `at_most_one`, see find_path_capabilities()),
 *      this sets the row count of the `limit` to the `scan` (requires runtime_feature::scan_limit).
 *      Otherwise, the operators between them may reduce or increase the number of rows,
 *      and this does not set the row count.
 *      This also does not set the row count if the `scan` already has one.
 *
 *      If the `limit` has sort keys, this sets the row count only if the `scan` already has the direction
 *      which provides the rows in the order of the sort keys,
 *      because taking the row count before sorting the rows changes the result.
 *
 *      The `limit` operators are retained, so that the downstream operators are not changed.
 * @attention The graph must not contain the `filter` operators whose condition is just `TRUE`
 *      (see remove_redundant_conditions()), because this treats them as the regular `filter` operators
 *      in the path from the `scan` to the `limit`.
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
