#pragma once

#include <takatori/relation/graph.h>
#include <takatori/plan/graph.h>

#include <yugawara/analyzer/details/step_plan_builder_options.h>

namespace yugawara::analyzer::details {

/**
 * @brief generates exchange steps and rewrite intermediate plan operators to use the generated exchange operations.
 * @details This may generate exchange steps when rewriting requires exchange operation; However, this has never
 *      generate any process steps and left rewritten operators in the source intermediate plan.
 *      Tos generate process steps, please call collect_process_steps().
 *
 *      The built exchange steps will be incomplete: they may not have valid exchange columns.
 *      But this will fill column mapping information of operations which obtains rows from indices.
 *      To complete exchange columns, please call collect_exchange_columns()
 *
 *      This also removes the redundant `limit` operators instead of generating exchange steps for them:
 *
 *      - `limit` operators which have neither the row count, the start position, sort keys nor group keys.
 *      - `limit` operators whose results are already provided by the nearest upstream `scan` or `limit` operator.
 *        That is, the `limit` is reachable from the upstream only through the operators which preserve the row order
 *        (see find_ordered_source()) or the other redundant `limit` operators,
 *        the `limit` does not have any group keys nor the start position, and satisfies the following:
 *
 *        - if the `limit` has the row count, the operators between them never increase the number of rows,
 *          and the upstream also has the row count which is less than or equal to it
 *          (requires runtime_feature::scan_limit for the upstream `scan`)
 *        - if the `limit` has sort keys, reading the upstream `scan` in its direction provides the rows in the order
 *          (requires runtime_feature::ordered_scan), or its sort keys are a prefix of the ones of the upstream `limit`
 *
 *      The upstream `limit` must not have any group keys nor the start position.
 *
 * @attention The source plan must not contain the `filter` operators whose condition is just `TRUE`
 *      (see remove_redundant_conditions()), because this treats them as the regular `filter` operators
 *      in the path between the `scan` and the `limit`, or between the `limit` and the `emit`.
 * @param source the source intermediate plan
 * @param destination the destination incomplete step plan
 * @param options the planning options
 * @see collect_process_steps()
 * @see collect_exchange_columns()
 * @note This rewrites intermediate plan operators by only simple rules.
 *      To obtain more optimized plan, please rewrite plan before/after this operation directly.
 *      For example, this always convert `intersection_relation (with distinct)` into `intersection_group`,
 *      but it can alternatively achieve to use semi-join operation.
 */
void collect_exchange_steps(
        ::takatori::relation::graph_type& source,
        ::takatori::plan::graph_type& destination,
        step_plan_builder_options const& options);

} // namespace yugawara::analyzer::details
