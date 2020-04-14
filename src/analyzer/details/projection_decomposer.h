#pragma once

#include <takatori/relation/graph.h>

#include <takatori/util/object_creator.h>

namespace yugawara::analyzer::details {

/**
 * @brief decomposes projections into multiple projections, which each projection only has a single column extension.
 * @param graph the target graph
 * @param creator the object creator
 */
void decompose_projections(
        ::takatori::relation::graph_type& graph,
        ::takatori::util::object_creator creator);

} // namespace yugawara::analyzer::details
