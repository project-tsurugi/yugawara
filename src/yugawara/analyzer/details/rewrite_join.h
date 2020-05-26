#pragma once

#include <takatori/relation/graph.h>
#include <takatori/util/object_creator.h>

#include <yugawara/storage/provider.h>
#include <yugawara/analyzer/index_estimator.h>

#include "flow_volume_info.h"

namespace yugawara::analyzer::details {

/**
 * @brief rewrites `join_relation` into `join_{scan,find} with suitable index.
 * @details This only rewrites `join_relation` without any key pairs nor endpoint conditions,
 *      and will retain `scan` with bounds or `find`.
 *      This never rewrite `join_relation` into `join_{scan,find}`.
 * @param graph the target graph
 * @param storage_provider the index provider
 * @param index_estimator the index cost estimator
 * @param flow_volume the flow volume information
 * @param creator the object creator
 */
void rewrite_join(
        ::takatori::relation::graph_type& graph,
        storage::provider const& storage_provider,
        analyzer::index_estimator const& index_estimator,
        flow_volume_info const& flow_volume,
        ::takatori::util::object_creator creator);

} // namespace yugawara::analyzer::details
