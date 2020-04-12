#pragma once

#include <yugawara/analyzer/details/step_planning_info.h>

#include <takatori/relation/graph.h>

#include <takatori/plan/graph.h>

namespace yugawara::analyzer::details {

/**
 * @brief collects use of exchange operations in each process step, and restore upstreams and downstreams of them.
 * @param destination the target step graph
 */
void collect_step_relations(::takatori::plan::graph_type& destination);

} // namespace yugawara::analyzer::details
