#pragma once

#include <takatori/relation/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief removes redundant sort keys in `limit` operations.
 * @details A sort key is redundant if the same variable appears in the preceding sort keys,
 *      because the rows which are tied on the preceding keys always have the same value of the variable.
 *      This removes such the sort keys regardless of their sort direction (e.g. `ORDER BY a, b, a DESC` -> `ORDER BY a, b`).
 *
 *      This only compares the sort key variables, so that this should be applied after removing variable aliases.
 * @param graph the target graph
 */
void remove_redundant_sort_keys(::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
