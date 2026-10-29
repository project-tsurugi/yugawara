#include <yugawara/analyzer/intermediate_plan_optimizer.h>

#include <utility>

#include "details/remove_variable_aliases.h"
#include "details/remove_redundant_sort_keys.h"
#include "details/remove_unused_stream_variables.h"
#include "details/remove_redundant_conditions.h"
#include "details/push_down_filters.h"
#include "details/push_down_limits.h"
#include "details/flow_volume_info.h"
#include "details/rewrite_join.h"
#include "details/collect_join_keys.h"
#include "details/rewrite_scan.h"
#include "details/collect_local_variables.h"
#include "details/decompose_prefix_match.h"
#include "details/decompose_filter.h"
#include "details/decompose_disjunction_range.h"
#include "details/absorb_limits.h"

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
    details::remove_variable_aliases(
            graph,
            options_.enable_external_variable_inlining());
    details::remove_redundant_sort_keys(graph);
    // details::decompose_projections(graph);
    details::remove_unused_stream_variables(graph);
    details::collect_local_variables(
            graph,
            options_.runtime_features().contains(runtime_feature::always_inline_scalar_local_variables));
    details::decompose_prefix_match(graph);
    if (options_.enable_disjunction_range_hinting()) {
        details::decompose_filter(graph);
        details::decompose_disjunction_range(graph);
    }

    details::push_down_selections(graph);
    details::push_down_limits(graph);
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
    if (options_.runtime_features().contains(runtime_feature::scan_limit)) {
        details::absorb_limits(graph);
    }
}

} // namespace yugawara::analyzer
