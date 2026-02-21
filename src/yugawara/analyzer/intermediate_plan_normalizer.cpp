#include <yugawara/analyzer/intermediate_plan_normalizer.h>

#include "details/expand_subquery.h"

namespace yugawara::analyzer {

void intermediate_plan_normalizer::operator()(takatori::relation::graph_type& graph) {
    details::expand_subquery(graph);
}

} // namespace yugawara::analyzer
