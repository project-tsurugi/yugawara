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
