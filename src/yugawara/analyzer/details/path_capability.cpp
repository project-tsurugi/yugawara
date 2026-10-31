#include "path_capability.h"

#include <memory>

#include <takatori/relation/project.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

path_capability_set find_path_capabilities(
        relation::expression::input_port_type const& input,
        relation::expression::output_port_type const& output) {
    auto&& owner = input.owner();
    if (std::addressof(owner) != std::addressof(output.owner())) {
        return {};
    }
    switch (owner.kind()) {
        case relation::project::tag:
            return {
                    path_capability::preserves_order,
                    path_capability::at_least_one,
                    path_capability::exactly_one,
            };
        default:
            return {};
    }
}

} // namespace yugawara::analyzer::details
