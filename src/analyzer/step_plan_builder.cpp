#include <yugawara/analyzer/step_plan_builder.h>

#include <takatori/relation/graph.h>

#include <takatori/plan/graph.h>

#include "details/collect_process_steps.h"
#include "details/collect_exchange_steps.h"
#include "details/collect_step_relations.h"

#include "details/collect_exchange_columns.h"
#include "details/rewrite_stream_variables.h"

namespace yugawara::analyzer {

namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

step_plan_builder::step_plan_builder(::takatori::util::object_creator creator) noexcept
    : options_(creator)
{}

step_plan_builder::step_plan_builder(options_type options) noexcept
    : options_(std::move(options))
{}

step_plan_builder::options_type& step_plan_builder::options() noexcept {
    return options_;
}

step_plan_builder::options_type const& step_plan_builder::options() const noexcept {
    return options_;
}

step_plan_builder& step_plan_builder::add(::takatori::relation::intermediate::join const& expr, join_info info) {
    options_.add(expr, info);
    return *this;
}

plan::graph_type step_plan_builder::operator()(relation::graph_type&& graph) const {
    ::takatori::plan::graph_type result { options_.get_object_creator() };

    // collect exchange steps and rewrite to step plan operators
    details::collect_exchange_steps(graph, result, options_);

    // collect process steps
    details::collect_process_steps(std::move(graph), result, options_.get_object_creator());

    // connect between processes and exchanges
    details::collect_step_relations(result);

    // fix exchange columns
    auto exchange_map = details::collect_exchange_columns(result, options_.get_object_creator());

    // rewrite all stream variables, and remove redundant columns
    details::rewrite_stream_variables(exchange_map, result, options_.get_object_creator());

    return result;
}

::takatori::util::object_creator step_plan_builder::get_object_creator() const noexcept {
    return options_.get_object_creator();
}

} // namespace yugawara::analyzer
