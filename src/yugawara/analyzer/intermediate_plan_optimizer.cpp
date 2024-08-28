#include <yugawara/analyzer/intermediate_plan_optimizer.h>

#include <utility>

#include "details/remove_redundant_stream_variables.h"
#include "details/remove_redundant_conditions.h"
#include "details/push_down_filters.h"
#include "details/flow_volume_info.h"
#include "details/rewrite_join.h"
#include "details/collect_join_keys.h"
#include "details/rewrite_scan.h"
#include "details/collect_local_variables.h"

namespace yugawara::analyzer {

intermediate_plan_optimizer::intermediate_plan_optimizer(options_type options) noexcept
    : options_(std::move(options))
{}

intermediate_plan_optimizer::options_type& intermediate_plan_optimizer::options() noexcept {
    return options_;
}

intermediate_plan_optimizer::options_type const& intermediate_plan_optimizer::options() const noexcept {
    return options_;
}

static constexpr details::collect_join_keys_feature_set compute_join_keys_features(runtime_feature_set const& features) {
    details::collect_join_keys_feature_set results;
    results.insert(details::collect_join_keys_feature::cogroup);
    if (features.contains(runtime_feature::broadcast_exchange)) {
        results.insert(details::collect_join_keys_feature::broadcast_find);
        if (features.contains(runtime_feature::broadcast_join_scan)) {
            results.insert(details::collect_join_keys_feature::broadcast_scan);
        }
    }
    return results;
}

void intermediate_plan_optimizer::operator()(::takatori::relation::graph_type& graph) {
    // details::decompose_projections(graph);
    details::remove_redundant_stream_variables(graph);
    details::collect_local_variables(
            graph,
            options_.runtime_features().contains(runtime_feature::always_inline_scalar_local_variables));
    details::push_down_selections(graph);
    // FIXME: auto flow_volume = details::reorder_join(...);
    details::flow_volume_info flow_volume {};
    if (options_.runtime_features().contains(runtime_feature::index_join)) {
        details::rewrite_join(
                graph,
                options_.index_estimator(),
                flow_volume,
                options_.runtime_features().contains(runtime_feature::index_join_scan));
    }
    details::collect_join_keys(
            graph,
            flow_volume,
            compute_join_keys_features(options_.runtime_features()));
    details::rewrite_scan(graph, options_.index_estimator());
    details::remove_redundant_conditions(graph);
}

} // namespace yugawara::analyzer
