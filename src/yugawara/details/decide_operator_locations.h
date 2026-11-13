#pragma once

#include <optional>

#include <tsl/hopscotch_map.h>

#include <takatori/relation/expression.h>
#include <takatori/relation/graph.h>

#include <yugawara/remote/location_info.h>
#include <yugawara/remote/symbol_location_query.h>

namespace yugawara::details {

/**
 * @brief the result type of decide_operator_locations().
 * @details Each entry maps an operator to the location where it is executed,
 *      or empty if the operator cannot be executed in any locations.
 */
using operator_location_map = ::tsl::hopscotch_map<
        ::takatori::relation::expression const*,
        std::optional<remote::location_info::location_id_type>>;

/**
 * @brief decides the single location where each operator in the given intermediate execution plan is executed.
 * @details The candidate locations of each operator are decided by element_location,
 *      and then they are unified into a single location from upstream to downstream, as follows:
 *
 *      1. If the operator has just one candidate, it is the location of the operator.
 *      2. If the candidates contain the local location, the local location is chosen.
 *      3. Otherwise, the candidate which is shared with the most upstream operators is chosen.
 *      4. If no candidates are shared with the upstream operators, the candidate with the smallest ID is chosen.
 *
 *      If the operator cannot be executed in any locations (that is, it has no candidates),
 *      its location is empty. Such operators are never counted as the upstream operators in the rule 3.
 *
 *      This does not modify the given plan.
 * @param graph the target intermediate execution plan
 * @param query the symbol location query
 * @return the location of each operator in the plan
 */
[[nodiscard]] operator_location_map decide_operator_locations(
        ::takatori::relation::graph_type const& graph,
        remote::symbol_location_query const& query);

} // namespace yugawara::details
