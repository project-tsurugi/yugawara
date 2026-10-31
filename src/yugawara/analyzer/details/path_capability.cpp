#include "path_capability.h"

#include <memory>

#include <takatori/relation/project.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/util/downcast.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::unsafe_downcast;

namespace {

path_capability_set find_join_capabilities(
        relation::intermediate::join const& expr,
        relation::expression::input_port_type const& input) {
    if (std::addressof(input) != std::addressof(expr.left())) {
        return {};
    }
    switch (expr.operator_kind()) {
        case relation::join_kind::left_outer:
            return {
                    path_capability::at_least_one,
            };
        case relation::join_kind::left_outer_at_most_one:
            return {
                    path_capability::at_least_one,
                    path_capability::exactly_one,
            };
        default:
            return {};
    }
}

} // namespace

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
        case relation::intermediate::join::tag:
            return find_join_capabilities(unsafe_downcast<relation::intermediate::join>(owner), input);
        default:
            return {};
    }
}

} // namespace yugawara::analyzer::details
