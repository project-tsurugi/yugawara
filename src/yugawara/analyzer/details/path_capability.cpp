#include "path_capability.h"

#include <memory>

#include <cstdlib>

#include <takatori/relation/apply.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/intermediate/escape.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::unsafe_downcast;

namespace {

path_capability_set find_apply_capabilities(relation::apply const& expr) {
    // NOTE: the row order depends on the implementation of table-valued functions, so that this never provides it
    switch (expr.operator_kind()) {
        case relation::apply_kind::cross:
            // NOTE: cross apply drops the input rows if the function returns no rows
            return {};
        case relation::apply_kind::outer:
            // NOTE: outer apply keeps the input rows even if the function returns no rows
            return {
                    path_capability::at_least_one,
            };
    }
    std::abort();
}

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
        case relation::apply::tag:
            return find_apply_capabilities(unsafe_downcast<relation::apply>(owner));
        case relation::join_find::tag:
            return find_index_join_capabilities(unsafe_downcast<relation::join_find>(owner).operator_kind());
        case relation::join_scan::tag:
            return find_index_join_capabilities(unsafe_downcast<relation::join_scan>(owner).operator_kind());
        case relation::intermediate::join::tag:
            return find_join_capabilities(unsafe_downcast<relation::intermediate::join>(owner), input);
        case relation::intermediate::limit::tag:
            // NOTE: limits are the boundaries of the paths, even if they only drop rows without changing the order;
            // the passes which handle limits decide whether or not other operators can go over them
            // (e.g. push_down_limits() merges them, and collect_exchange_steps() passes through some of them)
            return {};
        case relation::intermediate::escape::tag:
            // NOTE: escape is the boundary of the paths, even if it only renames the columns;
            // it separates the paths so that the optimizations do not mix over the merge points
            return {};
        default:
            return {};
    }
}

} // namespace yugawara::analyzer::details
