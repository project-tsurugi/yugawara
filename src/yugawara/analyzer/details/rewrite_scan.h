#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/analyzer/index_selection_parameters.h>

namespace yugawara::analyzer::details {

/**
 * @brief rewrites `scan` operations into `scan` / `find` with suitable indices.
 * @details This only rewrites `scan` without any scan conditions,
 *      and will retain `scan` with bounds or `find`.
 *      This never rewrite `join_relation` into `join_{scan,find}`.
 * @param graph the target graph
 * @param index_estimator the index estimator
 * @param parameters the parameters to choose an index from the estimations
 */
void rewrite_scan(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator,
        index_selection_parameters const& parameters);

} // namespace yugawara::analyzer::details
