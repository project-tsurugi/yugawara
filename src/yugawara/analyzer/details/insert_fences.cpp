#include "insert_fences.h"

#include <takatori/util/string_builder.h>

#include <yugawara/extension/relation/fence.h>

#include <yugawara/remote/details/decide_operator_locations.h>

namespace yugawara::analyzer::details {

namespace trelation = ::takatori::relation;

using ::takatori::util::string_builder;

using location_id_type = remote::location_info::location_id_type;
using diagnostic_type = diagnostic<intermediate_plan_normalizer_code>;
using remote::details::operator_location_map;

namespace {

struct boundary {
    trelation::expression::output_port_type* upstream;
    trelation::expression::input_port_type* downstream;
    location_id_type upstream_location;
    location_id_type downstream_location;
};

[[nodiscard]] std::vector<diagnostic_type> collect_undecided(
        trelation::graph_type const& graph,
        operator_location_map const& locations) {
    std::vector<diagnostic_type> results {};
    for (auto&& expression : graph) {
        if (!locations.at(std::addressof(expression))) {
            results.emplace_back(
                    intermediate_plan_normalizer_code::empty_operation_location,
                    string_builder {}
                            << "there are no locations where the operator can be executed: "
                            << expression.kind()
                            << string_builder::to_string,
                    expression.region());
        }
    }
    return results;
}

} // namespace

std::vector<diagnostic_type> insert_fences(
        trelation::graph_type& graph,
        remote::location_options const& options) {
    auto locations = remote::details::decide_operator_locations(graph, options);
    if (auto diagnostics = collect_undecided(graph, locations); !diagnostics.empty()) {
        // NOTE: inserts no fences, so that the caller can compile the plan as if the locations were ignored
        return diagnostics;
    }

    // NOTE: collect the boundaries before inserting fences, because the insertion modifies the graph
    std::vector<boundary> boundaries {};
    for (auto&& expression : graph) {
        auto upstream_location = *locations.at(std::addressof(expression));
        for (auto&& output : expression.output_ports()) {
            auto opposite = output.opposite();
            if (!opposite) {
                continue;
            }
            auto downstream_location = *locations.at(std::addressof(opposite->owner()));
            if (upstream_location != downstream_location) {
                boundaries.emplace_back(boundary {
                        std::addressof(output),
                        opposite.get(),
                        upstream_location,
                        downstream_location,
                });
            }
        }
    }

    /*
     * .. - upstream - downstream - ..
     * =>
     * .. - upstream - fence{upstream_location, downstream_location} - downstream - ..
     */
    for (auto&& entry : boundaries) {
        auto&& fence = graph.insert(extension::relation::fence {
                entry.upstream_location,
                entry.downstream_location,
        });
        entry.upstream->reconnect_to(fence.input());
        fence.output() >> *entry.downstream;
    }
    return {};
}

} // namespace yugawara::analyzer::details
