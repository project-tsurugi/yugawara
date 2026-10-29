#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/runtime_feature.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes redundant `limit` operators.
 * @details This removes the following `limit` operators:
 *
 *      - `limit` operators which have neither the row count, the start position, sort keys nor group keys.
 *      - `limit` operators whose results are already provided by the upstream `scan` operator.
 *        That is, the `limit` is reachable from the `scan` only through the operators which preserve the row order
 *        and never increase the number of rows (see find_ordered_destination()), and the `limit` does not have any
 *        group keys nor the start position, and satisfies the following:
 *
 *        - if the `limit` has the row count, the `scan` also has the row count which is less than or equal to it
 *          (requires runtime_feature::scan_limit)
 *        - if the `limit` has sort keys, reading the `scan` in its direction provides the rows in the order
 *          (requires runtime_feature::ordered_scan)
 *
 * @param graph the target graph
 * @param features the available runtime features
 * @see absorb_limits()
 */
void remove_redundant_limits(
        ::takatori::relation::graph_type& graph,
        runtime_feature_set const& features);

} // namespace yugawara::analyzer::details
