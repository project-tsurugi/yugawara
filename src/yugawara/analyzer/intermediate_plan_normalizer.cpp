#include <yugawara/analyzer/intermediate_plan_normalizer.h>

#include "details/expand_subquery.h"
#include "details/insert_fences.h"

namespace yugawara::analyzer {

remote::location_options& intermediate_plan_normalizer::location_options() noexcept {
    return location_options_;
}

remote::location_options const& intermediate_plan_normalizer::location_options() const noexcept {
    return location_options_;
}

std::vector<intermediate_plan_normalizer::diagnostic_type> intermediate_plan_normalizer::operator()(
        ::takatori::relation::graph_type& graph) {
    if (auto diagnostics = details::expand_subquery(graph); !diagnostics.empty()) {
        return diagnostics;
    }
    if (location_options_.symbol_location_query()) {
        // NOTE: fences must be placed before optimization, to prevent moving operators across the locations
        auto diagnostics = details::insert_fences(graph, location_options_);
        if (!diagnostics.empty() && !location_options_.allow_empty_operation_location()) {
            return diagnostics;
        }
        // NOTE: if empty locations are allowed, continues without fences, as if the locations were ignored
    }
    return {};
}

} // namespace yugawara::analyzer
