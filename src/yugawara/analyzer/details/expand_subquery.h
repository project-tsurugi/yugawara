#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief expands relation subqueries into the outer query.
 * @details this operation will apply recursively to handle nested subqueries.
 * @param graph the target query graph
 * @note if the graph contains no subqueries, this function will do nothing.
 */
void expand_subquery(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
