#include <yugawara/analyzer/intermediate_plan_normalizer.h>

#include <takatori/util/assertion.h>

#include <yugawara/remote/details/insert_fences.h>

#include "details/expand_subquery.h"

namespace yugawara::analyzer {

namespace {

[[nodiscard]] std::vector<intermediate_plan_normalizer::diagnostic_type> insert_fences(
        ::takatori::relation::graph_type& graph,
        remote::location_options const& options) {
    auto diagnostics = remote::details::insert_fences(graph, options);
    if (diagnostics.empty() || options.allow_empty_operation_location()) {
        // NOTE: if empty locations are allowed, continues without fences, as if the locations were ignored
        return {};
    }
    std::vector<intermediate_plan_normalizer::diagnostic_type> results {};
    results.reserve(diagnostics.size());
    for (auto&& d : diagnostics) {
        BOOST_ASSERT(d.code() == compiler_code::empty_operation_location); // NOLINT
        results.emplace_back(
                intermediate_plan_normalizer_code::empty_operation_location,
                d.message(),
                d.location());
    }
    return results;
}

} // namespace

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
        if (auto diagnostics = insert_fences(graph, location_options_); !diagnostics.empty()) {
            return diagnostics;
        }
    }
    return {};
}

} // namespace yugawara::analyzer
