#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief places `filter` operators to upstream.
 * @details This sometimes merges or decomposes predicates into join like operations.
 *      For left outer joins (including left outer joins at most one) and anti joins, this pushes down
 *      the terms of the join condition which only refer the right columns into the right input,
 *      and keeps the other terms in the join condition.
 * @param graph the target graph
 */
void push_down_selections(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
