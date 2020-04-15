#pragma once

#include <takatori/relation/graph.h>

#include <takatori/util/object_creator.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes redundant declaration of stream variables.
 * @param graph the target graph
 * @param creator the object creator
 */
void remove_redundant_stream_variables(
        ::takatori::relation::graph_type& graph,
        ::takatori::util::object_creator creator);

} // namespace yugawara::analyzer::details
