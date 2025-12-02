#pragma once

#include <functional>

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief decomposes filter operations in the graph.
 * @detail this operation split filters with conjunctive predicates into multiple filter operations.
 * @param graph the source graph
 */
void decompose_filter(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
