#pragma once

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>

#include "estimate_index.h"

namespace yugawara::analyzer::details {

/**
 * @brief rewrites the scan operation to access the index of the given candidate.
 * @details If the candidate is estimated as `find`, this creates a new `find` operation into the given graph,
 *      and then reconnects the downstream of the scan to it.
 *      The orphaned scan operation must be removed by the caller.
 *
 *      Otherwise, this replaces the source of the scan with the index, and then sets its end-points.
 *
 *      In both cases, this purges the terms of the candidate from their original conditions,
 *      and replaces them with `TRUE` (see index_candidate).
 * @param expr the target scan operation, which must not have any end-points
 * @param candidate the index candidate, which was estimated for the target scan operation
 * @param graph the graph to put the new operations,
 *      which can be different from the graph of the target scan operation
 */
void apply_index_to_scan(
        ::takatori::relation::scan& expr,
        index_candidate& candidate,
        ::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
