#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief decomposes projections into multiple projections, which each projection only has a single column extension.
 * @param graph the target graph
 */
void decompose_projections(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
