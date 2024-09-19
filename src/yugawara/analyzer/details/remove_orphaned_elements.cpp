#include "remove_orphaned_elements.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

namespace {

[[nodiscard]] bool has_orphaned_port(relation::expression const& expr) {
    for (auto&& port : expr.input_ports()) { // NOLINT(*-use-anyofallof)
        if (!port.opposite()) {
            return true;
        }
    }
    for (auto&& port : expr.output_ports()) { // NOLINT(*-use-anyofallof)
        if (!port.opposite()) {
            return true;
        }
    }
    return false;
}

} // namespace

void remove_orphaned_elements(relation::graph_type &graph) {
    // NOTE: remove orphaned operators
    for (auto it = graph.begin(); it != graph.end();) {
        auto&& expr = *it;
        if (has_orphaned_port(expr)) {
            it = graph.erase(it);
            continue;
        }
        ++it;
    }
}

} // namespace yugawara::analyzer::details
