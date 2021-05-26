#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes redundant declaration of stream variables.
 * @param graph the target graph
 */
void remove_redundant_stream_variables(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
