#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes orphaned elements in the operator graph.
 * @param graph the target graph
 */
void remove_orphaned_elements(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
