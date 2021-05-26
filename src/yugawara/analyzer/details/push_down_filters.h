#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief places `filter` operators to upstream.
 * @details This sometimes merges or decomposes predicates into join like operations.
 * @param graph the target graph
 */
void push_down_selections(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
