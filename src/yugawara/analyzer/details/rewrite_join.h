#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/analyzer/index_estimator.h>

#include "flow_volume_info.h"

namespace yugawara::analyzer::details {

/**
 * @brief rewrites `join_relation` into `join_{scan,find} with suitable index.
 * @details This only rewrites `join_relation` without any key pairs nor endpoint conditions,
 *      and will retain `scan` with bounds or `find`.
 *      This never rewrite `join_relation` into `join_{scan,find}`.
 * @param graph the target graph
 * @param index_estimator the index cost estimator
 * @param flow_volume the flow volume information
 */
void rewrite_join(
        ::takatori::relation::graph_type& graph,
        analyzer::index_estimator const& index_estimator,
        flow_volume_info const& flow_volume);

} // namespace yugawara::analyzer::details
