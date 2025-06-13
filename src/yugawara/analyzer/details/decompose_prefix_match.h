#pragma once

#include <takatori/relation/graph.h>

#include <takatori/scalar/expression.h>

#include <takatori/util/ownership_reference.h>

namespace yugawara::analyzer::details {

void decompose_prefix_match(::takatori::relation::graph_type& graph);

void decompose_prefix_match(::takatori::util::ownership_reference<::takatori::scalar::expression>& expr);

} // namespace yugawara::analyzer::details
