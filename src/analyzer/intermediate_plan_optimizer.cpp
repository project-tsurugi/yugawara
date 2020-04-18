#include <yugawara/analyzer/intermediate_plan_optimizer.h>

#include "details/remove_redundant_stream_variables.h"
#include "details/remove_redundant_conditions.h"
#include "details/push_down_filters.h"

namespace yugawara::analyzer {

intermediate_plan_optimizer::intermediate_plan_optimizer(::takatori::util::object_creator creator) noexcept
    : options_(creator)
{}

intermediate_plan_optimizer::intermediate_plan_optimizer(options_type options) noexcept
    : options_(options)
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
    details::remove_redundant_conditions(graph);

    // FIXME: impl
}

} // namespace yugawara::analyzer
