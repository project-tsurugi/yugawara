#include <yugawara/analyzer/step_plan_builder.h>

#include <stdexcept>

#include <takatori/relation/graph.h>

#include <takatori/plan/graph.h>

#include "details/process_step_collector.h"
#include "details/exchange_step_collector.h"
#include "details/step_relation_collector.h"

#include "details/exchange_column_collector.h"
#include "details/stream_variable_rewriter.h"

namespace yugawara::analyzer {

namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

step_plan_builder::step_plan_builder(::takatori::util::object_creator creator) noexcept
    : info_(creator)
{}

step_plan_builder::step_plan_builder(step_plan_builder::planning_info info) noexcept
    : info_(std::move(info))
{}

step_plan_builder::planning_info& step_plan_builder::info() noexcept {
    return info_;
}

step_plan_builder::planning_info const& step_plan_builder::info() const noexcept {
    return info_;
}

step_plan_builder& step_plan_builder::add(::takatori::relation::intermediate::join const& expr, join_info info) {
    info_.add(expr, info);
    return *this;
}

plan::graph_type step_plan_builder::build(relation::graph_type&& graph) noexcept {
    ::takatori::plan::graph_type result { info_.get_object_creator() };

    // collect exchange steps and rewrite to step plan operators
    details::collect_exchange_steps(graph, result, info_);

    // collect process steps
    details::collect_process_steps(std::move(graph), result, info_.get_object_creator());

    // connect between processes and exchanges
    details::collect_step_relations(result);

    // fix exchange columns
    auto exchange_map = details::collect_exchange_columns(result, info_.get_object_creator());

    // rewrite all stream variables, and remove redundant columns
    details::rewrite_stream_variables(exchange_map, result, info_.get_object_creator());

    return result;
}

::takatori::util::object_creator step_plan_builder::get_object_creator() const noexcept {
    return info_.get_object_creator();
}

} // namespace yugawara::analyzer
