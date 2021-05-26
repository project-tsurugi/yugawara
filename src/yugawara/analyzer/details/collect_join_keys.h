#pragma once

#include <takatori/relation/graph.h>
#include <takatori/util/enum_set.h>

#include "flow_volume_info.h"

namespace yugawara::analyzer::details {

enum class collect_join_keys_feature {
    cogroup,
    broadcast_find,
    broadcast_scan, // implies broadcast_find
};

using collect_join_keys_feature_set = ::takatori::util::enum_set<
        collect_join_keys_feature,
        collect_join_keys_feature::cogroup,
        collect_join_keys_feature::broadcast_scan>;

constexpr collect_join_keys_feature_set collect_join_keys_feature_universe {
        collect_join_keys_feature::cogroup,
        collect_join_keys_feature::broadcast_find,
        collect_join_keys_feature::broadcast_scan,
};

/**
 * @brief rewrites search keys of `join_relation`.
 * @details This analyzes optional condition of join operation, and decompose and re-organize it into
 *      equivalent search endpoints (`lower` and `upper` property).
 *
 *      This will ignore `join_{find,scan}`, or `join_relation` with configured endpoints.
 *      If you want to perform join on indices, please use rewrite_join() before this operation.
 * @param graph the target graph
 * @param flow_volume the flow volume information
 * @param features the available feature set, NLJ is always allowed
 */
void collect_join_keys(
        ::takatori::relation::graph_type& graph,
        flow_volume_info const& flow_volume,
        collect_join_keys_feature_set features);

} // namespace yugawara::analyzer::details
