#pragma once

#include <takatori/relation/graph.h>

#include <takatori/util/object_creator.h>

namespace yugawara::analyzer::details {

/**
 * @brief places `filter` operators to upstream.
 * @details This sometimes merges or decomposes predicates into join like operations.
 * @param graph the target graph
 * @param creator the object creator
 */
void push_down_selections(
        ::takatori::relation::graph_type& graph,
        ::takatori::util::object_creator creator);

} // namespace yugawara::analyzer::details
