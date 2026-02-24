#include <yugawara/analyzer/intermediate_plan_normalizer.h>

#include "details/expand_subquery.h"

namespace yugawara::analyzer {


std::vector<intermediate_plan_normalizer::diagnostic_type> intermediate_plan_normalizer::operator()(
        ::takatori::relation::graph_type& graph) {
    if (auto diagnostics = details::expand_subquery(graph); !diagnostics.empty()) {
        return diagnostics;
    }
    return {};
}

} // namespace yugawara::analyzer
