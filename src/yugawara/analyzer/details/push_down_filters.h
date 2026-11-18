#pragma once

#include <optional>

#include <takatori/relation/graph.h>

#include <yugawara/remote/details/element_location.h>

namespace yugawara::analyzer::details {

/**
 * @brief places `filter` operators to upstream.
 * @details This sometimes merges or decomposes predicates into join like operations.
 *      For left outer joins (including left outer joins at most one) and anti joins, this pushes down
 *      the terms of the join condition which only refer the right columns into the right input,
 *      and keeps the other terms in the join condition.
 *
 *      This never moves the terms across fences (extension::relation::fence), unless the locations of elements
 *      are given. If they are given, this moves each term into the upstream of a fence only if a `filter` operator
 *      with the term can be executed in the upstream location of the fence:
 *      that is, `filter` operators can be executed in the location,
 *      and the term can be evaluated in the location.
 *
 *      If the locations of elements are given, for each `project` operator, this places the terms which use
 *      the columns defined by the operator just after it, and puts the terms whose `filter` operators can be executed
 *      in the upstream location of the nearest fence in the upstream of the operator (only through `project` and
 *      `filter` operators) next to the operator, preserving the order of terms in each group.
 *      This is for pull_up_fence(), which moves fences across `project` and `filter` operators.
 * @param graph the target graph
 * @param locations the locations of elements, or empty to keep all terms in the downstream of fences
 */
void push_down_selections(
        ::takatori::relation::graph_type& graph,
        std::optional<remote::details::element_location> locations = {});

} // namespace yugawara::analyzer::details
