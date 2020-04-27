#pragma once

#include <takatori/relation/graph.h>
#include <takatori/util/object_creator.h>

#include <yugawara/storage/provider.h>
#include <yugawara/analyzer/index_estimator.h>

namespace yugawara::analyzer::details {

/**
 * @brief rewrites `join_relation` into `join_{scan,find} with suitable index.
 * @details This only rewrites `join_relation` without any key pairs nor endpoint conditions,
 *      and will retain `scan` with bounds or `find`.
 *      This never rewrite `join_relation` into `join_{scan,find}`.
 * @param graph
 * @param storage_provider
 * @param index_estimator
 * @param creator
 */
void rewrite_join(
        ::takatori::relation::graph_type& graph,
        storage::provider const& storage_provider,
        class index_estimator& index_estimator,
        ::takatori::util::object_creator creator);

} // namespace yugawara::analyzer::details
