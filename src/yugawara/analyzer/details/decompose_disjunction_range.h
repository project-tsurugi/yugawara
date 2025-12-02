#pragma once

#include <memory>
#include <vector>

#include <takatori/relation/graph.h>

#include <takatori/scalar/expression.h>

namespace yugawara::analyzer::details {

/**
 * @brief extracts value range for individual columns in disjunctive predicates.
 * @param graph the target graph
 * @note this requires the disjunction operator (conditional OR) on the top level of each filter expression.
 *     please decompose conjunction operators (conditional AND) before calling this function.
 */
void decompose_disjunction_range(::takatori::relation::graph_type& graph);

/**
 * @brief extracts value range for individual columns in disjunctive predicates.
 * @param expr the target expression
 * @return the extracted expressions
 * @note this requires the disjunction operator (conditional OR) on the top level of the expression.
 *     please decompose conjunction operators (conditional AND) before calling this function.
 */
[[nodiscard]] std::vector<std::unique_ptr<::takatori::scalar::expression>> decompose_disjunction_range_collect(
        ::takatori::scalar::expression const& expr);

} // namespace yugawara::analyzer::details
