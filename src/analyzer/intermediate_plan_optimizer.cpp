#include <yugawara/analyzer/intermediate_plan_optimizer.h>

#include <utility>

#include "details/remove_redundant_stream_variables.h"
#include "details/remove_redundant_conditions.h"
#include "details/push_down_filters.h"
#include "details/flow_volume_info.h"
#include "details/rewrite_join.h"
#include "details/collect_join_keys.h"
#include "details/rewrite_scan.h"

namespace yugawara::analyzer {

constexpr details::collect_join_keys_feature_set collect_join_keys_features {
        details::collect_join_keys_feature::cogroup,
        details::collect_join_keys_feature::broadcast_find,
        details::collect_join_keys_feature::broadcast_scan,
};

intermediate_plan_optimizer::intermediate_plan_optimizer(::takatori::util::object_creator creator) noexcept
    : options_(creator)
{}

intermediate_plan_optimizer::intermediate_plan_optimizer(options_type options) noexcept
    : options_(std::move(options))
{}

intermediate_plan_optimizer::options_type& intermediate_plan_optimizer::options() noexcept {
    return options_;
}

intermediate_plan_optimizer::options_type const& intermediate_plan_optimizer::options() const noexcept {
    return options_;
}

void intermediate_plan_optimizer::operator()(::takatori::relation::graph_type& graph) {
    // details::decompose_projections(graph, get_object_creator());
    details::remove_redundant_stream_variables(graph, get_object_creator());
    details::push_down_selections(graph, get_object_creator());
    // FIXME: auto flow_volume = details::reorder_join(...);
    details::flow_volume_info flow_volume { get_object_creator() };
    details::rewrite_join(graph, options_.storage_provider(), options_.index_estimator(), flow_volume, get_object_creator());
    details::collect_join_keys(graph, flow_volume, collect_join_keys_features, get_object_creator());
    details::rewrite_scan(graph, options_.storage_provider(), options_.index_estimator(), get_object_creator());
    details::remove_redundant_conditions(graph);
}

} // namespace yugawara::analyzer
