#pragma once

#include <takatori/relation/graph.h>
#include <takatori/plan/graph.h>

#include <yugawara/extension/relation/subquery.h>

#include "exchange_column_info_map.h"
#include "stream_variable_rewriter_context.h"

namespace yugawara::analyzer::details {

/**
 * @brief rewrites all stream variables in the given step plan.
 * @details This also removes unused stream variables and exchange columns.
 *      This accepts `escape` operators in process steps, but they will be removed in this operation.
 * @param exchange_map
 * @param graph
 * @pre exchange_column_collector
 */
void rewrite_stream_variables(
        exchange_column_info_map& exchange_map,
        ::takatori::plan::graph_type& graph);

/**
 * @brief rewrite all stream variables in the given subquery.
 * @details this does not rewrite variables recursively, that is,
 *      the nested relation subqueries will keep their original variables.
 *      Therefore, other than relation subqueries, scalar or predicate, will be rewritten their variables.
 * @param subquery the target subquery
 */
void rewrite_stream_variables(extension::relation::subquery& subquery);

/**
 * @brief rewrite all stream variables in the given query graph.
 * @details this does not rewrite variables recursively, that is,
 *      the nested relation subqueries will keep their original variables.
 *      Therefore, other than relation subqueries, scalar or predicate, will be rewritten their variables.
 * @param context the context for stream variable rewriting, which will be updated by this operation
 * @param graph the target query graph
 */
void rewrite_stream_variables(
        stream_variable_rewriter_context& context,
        ::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
