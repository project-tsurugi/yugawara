#include <yugawara/analyzer/intermediate_plan_optimizer.h>

#include "details/decompose_projections.h"
#include "details/remove_redundant_stream_variables.h"

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
    details::decompose_projections(graph, get_object_creator());
    details::remove_redundant_stream_variables(graph, get_object_creator());

    // FIXME: impl
}

} // namespace yugawara::analyzer
