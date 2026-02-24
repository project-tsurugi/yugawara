#pragma once

#include <vector>

#include <takatori/relation/graph.h>

#include <yugawara/diagnostic.h>
#include <yugawara/analyzer/intermediate_plan_normalizer_code.h>

namespace yugawara::analyzer::details {

/**
 * @brief expands relation subqueries into the outer query.
 * @details this operation will apply recursively to handle nested subqueries.
 * @param graph the target query graph
 * @return diagnostics if any error is found during the expansion
 * @note if the graph contains no subqueries, this function will do nothing.
 */
[[nodiscard]] std::vector<diagnostic<intermediate_plan_normalizer_code>> expand_subquery(
        ::takatori::relation::graph_type& graph);

} // namespace yugawara::analyzer::details
