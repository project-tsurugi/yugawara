#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes stream variable aliases from the operator graph.
 * @details this removes unnecessary variable copies in `project` operators.
 *     Note that, this remains the `escape` operators that are necessary for other optimization.
 * @param graph the operator graph to optimize
 * @param extract_external_variables if true, also extracts external variables from the aliases
 * @note This keeps the graph structure, even if some `project` operators become empty columns.
 */
void remove_variable_aliases(::takatori::relation::graph_type& graph, bool extract_external_variables = false);

} // namespace yugawara::analyzer::details
