#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes redundant conditions in filters and joins.
 * @param graph the target graph
 */
void remove_redundant_conditions(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
