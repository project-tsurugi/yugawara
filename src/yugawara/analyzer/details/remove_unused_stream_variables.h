#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes unused declaration of stream variables.
 * @details this also remove the following redundant operators:
 *   - `project` operator which all columns are unused in downstream
 *   - `identity` operator which the generated identity column is unused in downstream
 * @param graph the target graph
 */
void remove_unused_stream_variables(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
