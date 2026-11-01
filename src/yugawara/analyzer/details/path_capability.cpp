#include "path_capability.h"

#include <memory>

#include <cstdlib>

#include <takatori/relation/buffer.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
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
        case relation::join_kind::inner:
            // NOTE: inner joins may drop or duplicate the left input rows
            return {};
        case relation::join_kind::left_outer:
            return {
                    path_capability::at_least_one,
            };
        case relation::join_kind::left_outer_at_most_one:
            return {
                    path_capability::at_least_one,
                    path_capability::at_most_one,
            };
        case relation::join_kind::full_outer:
            // NOTE: full outer joins make each left input row one or more output rows,
            // but they also emit the rows only from the right input, whose left columns are null.
            // That is, the path does not keep the contents of the columns (i.e. not "preserve_columns"),
            // so that it also loses `at_least_one`.
            return {};
        case relation::join_kind::semi:
        case relation::join_kind::anti:
            return {
                    path_capability::at_most_one,
            };
    }
    std::abort();
}

path_capability_set find_buffer_capabilities(
        relation::buffer const& expr,
        relation::expression::output_port_type const& output) {
    // NOTE: the secondary outputs only need the rows which the downstream of the primary output requires
    if (!expr.stop_with_primary() || std::addressof(output) != std::addressof(expr.primary_output())) {
        return {};
    }
    return {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::at_most_one,
    };
}

path_capability_set find_index_join_capabilities(relation::join_kind kind) {
    // NOTE: index joins emit the result rows of each left input row in order
    switch (kind) {
        case relation::join_kind::inner:
            return {
                    path_capability::preserves_order,
            };
        case relation::join_kind::left_outer:
            return {
                    path_capability::preserves_order,
                    path_capability::at_least_one,
            };
        case relation::join_kind::left_outer_at_most_one:
            return {
                    path_capability::preserves_order,
                    path_capability::at_least_one,
                    path_capability::at_most_one,
            };
        case relation::join_kind::full_outer:
            // NOTE: index joins never be full outer joins, but we treat them as same as `intermediate::join`
            return {};
        case relation::join_kind::semi:
        case relation::join_kind::anti:
            return {
                    path_capability::preserves_order,
                    path_capability::at_most_one,
            };
    }
    std::abort();
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
        case relation::filter::tag:
            return {
                    path_capability::preserves_order,
                    path_capability::at_most_one,
            };
        case relation::project::tag:
            return {
                    path_capability::preserves_order,
                    path_capability::at_least_one,
                    path_capability::at_most_one,
            };
        case relation::buffer::tag:
            return find_buffer_capabilities(unsafe_downcast<relation::buffer>(owner), output);
        case relation::join_find::tag:
            return find_index_join_capabilities(unsafe_downcast<relation::join_find>(owner).operator_kind());
        case relation::join_scan::tag:
            return find_index_join_capabilities(unsafe_downcast<relation::join_scan>(owner).operator_kind());
        case relation::intermediate::join::tag:
            return find_join_capabilities(unsafe_downcast<relation::intermediate::join>(owner), input);
        default:
            return {};
    }
}

} // namespace yugawara::analyzer::details
