#pragma once

#include <vector>

#include <takatori/relation/graph.h>

#include <yugawara/compiler_code.h>
#include <yugawara/diagnostic.h>
#include <yugawara/remote/symbol_location_query.h>

namespace yugawara::details {

/**
 * @brief inserts fences into the boundaries of the execution locations in the given intermediate execution plan.
 * @details This decides the location of each operator by decide_operator_locations(),
 *      and then inserts a fence into each connection between two operators which are executed in different locations.
 *      Each fence holds the locations of its upstream and downstream operators.
 *      This never inserts consecutive fences, because each connection receives at most one fence.
 *
 *      If there are operators which cannot be executed in any locations,
 *      this does not insert any fences and leaves the plan as is, and then reports such operators.
 *      The caller may treat them as errors, or ignore them to compile the plan as if the locations were ignored.
 * @param graph the target intermediate execution plan, must be normalized
 * @param query the symbol location query
 * @return empty if fences are successfully inserted
 * @return diagnostics with compiler_code::empty_operation_location
 *      if there are operators which cannot be executed in any locations, and then the plan is not modified
 */
[[nodiscard]] std::vector<diagnostic<compiler_code>> insert_fences(
        ::takatori::relation::graph_type& graph,
        remote::symbol_location_query const& query);

} // namespace yugawara::details
