#pragma once

#include <takatori/scalar/expression.h>

#include <takatori/relation/graph.h>

#include <takatori/util/ownership_reference.h>

namespace yugawara::analyzer::details {

/**
 * @brief collects local variable declarations and process them.
 * @brief graph the target graph
 * @brief always_inline whether to inline all local variables
 */
void collect_local_variables(
        ::takatori::relation::graph_type& graph,
        bool always_inline = false);

/**
 * @brief collects local variable declarations and process them.
 * @brief expression the target expression
 * @brief always_inline whether to inline all local variables
 */
void collect_local_variables(
        ::takatori::util::ownership_reference<::takatori::scalar::expression> expression,
        bool always_inline = false);

} // namespace yugawara::analyzer::details
