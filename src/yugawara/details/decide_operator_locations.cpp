#include "decide_operator_locations.h"

#include <cstddef>
#include <optional>

#include "element_location.h"

namespace yugawara::details {

namespace trelation = ::takatori::relation;

using location_id_type = remote::location_info::location_id_type;
using location_id_set_type = remote::location_info::location_id_set_type;

namespace {

[[nodiscard]] std::optional<location_id_type> choose(
        location_id_set_type const& candidates,
        trelation::expression const& expression,
        operator_location_map const& decided) {
    if (candidates.empty()) {
        return {};
    }
    if (candidates.size() == 1) {
        return *candidates.begin();
    }
    if (candidates.find(remote::location_info::local_location_id) != candidates.end()) {
        return remote::location_info::local_location_id;
    }

    // chooses the candidate which is shared with the most upstream operators
    location_id_type result = *candidates.begin(); // the smallest ID
    std::size_t result_count = 0;
    for (auto candidate : candidates) {
        std::size_t count = 0;
        for (auto&& input : expression.input_ports()) {
            if (auto upstream = input.opposite()) {
                if (auto it = decided.find(std::addressof(upstream->owner()));
                        it != decided.end() && it->second == candidate) {
                    ++count;
                }
            }
        }
        if (count > result_count) {
            result = candidate;
            result_count = count;
        }
    }
    return result;
}

} // namespace

operator_location_map decide_operator_locations(
        trelation::graph_type const& graph,
        remote::symbol_location_query const& query) {
    element_location locations { query };
    operator_location_map results {};
    results.reserve(graph.size());
    trelation::sort_from_upstream(graph, [&](trelation::expression const& expression) {
        auto location = choose(locations(expression), expression, results);
        results.emplace(std::addressof(expression), location);
    });
    return results;
}

} // namespace yugawara::details
