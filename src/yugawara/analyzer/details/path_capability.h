#pragma once

#include <ostream>
#include <string_view>

#include <cstdlib>

#include <takatori/relation/expression.h>

#include <takatori/util/enum_set.h>

namespace yugawara::analyzer::details {

/**
 * @brief represents a capability of the path from an input port to an output port of the same operator.
 * @details The capabilities are used to decide whether `limit` operators can be placed to upstream of the operator,
 *      and whether the row order can be traced over the operator.
 *      If a path has both `at_least_one` and `at_most_one`, each input row becomes exactly one output row.
 */
enum class path_capability {
    /**
     * @brief the order of output rows is consistent with the order of input rows.
     * @details The output rows derived from the same input row may be contiguous.
     */
    preserves_order,

    /// @brief each input row becomes one or more output rows, with keeping the values of its columns.
    at_least_one,

    /// @brief each input row becomes zero or one output row, with keeping the values of its columns.
    at_most_one,
};

/**
 * @brief a set of path_capability.
 */
using path_capability_set = ::takatori::util::enum_set<
        path_capability,
        path_capability::preserves_order,
        path_capability::at_most_one>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(path_capability value) noexcept {
    using namespace std::string_view_literals;
    using kind = path_capability;
    switch (value) {
        case kind::preserves_order: return "preserves_order"sv;
        case kind::at_least_one: return "at_least_one"sv;
        case kind::at_most_one: return "at_most_one"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, path_capability value) {
    return out << to_string_view(value);
}

/**
 * @brief the properties of the runtime which affect path capabilities.
 * @details The default values never assume anything about the runtime.
 * @see find_path_capabilities()
 */
struct path_capability_options {
    /// @brief whether or not `apply` emits rows in the order of its input rows.
    bool apply_preserves_order { false };
};

/**
 * @brief returns the capabilities of the path from the input port to the output port.
 * @details This only knows the following paths, and returns an empty set for the others:
 *
 *      - `filter`
 *      - `project`
 *      - the left input of joins (`intermediate::join`)
 *      - the left input of index joins (`join_find` and `join_scan`)
 *      - the primary output of `buffer` which can stop with the primary output
 *        (see `buffer::stop_with_primary()`)
 *      - `apply` (at least one for outer apply only, and it preserves the row order only if
 *        path_capability_options::apply_preserves_order is enabled)
 *      - each input of `intermediate::union_` (union all only, and it never preserves the row order)
 *
 *      The callers must treat the empty set as the path has no capabilities.
 *
 *      The paths of `intermediate::limit` always have no capabilities,
 *      because the other passes treat them as the boundaries of the paths.
 * @attention This never inspects the condition of `filter`, so that the `filter` whose condition is just `TRUE`
 *      has the same capabilities as the regular one (see remove_redundant_conditions()).
 * @param input the input port of the operator
 * @param output the output port of the same operator
 * @param options the properties of the runtime
 * @return the capabilities of the path
 * @return empty set if the operator is not known, or the ports do not belong to the same operator
 */
[[nodiscard]] path_capability_set find_path_capabilities(
        ::takatori::relation::expression::input_port_type const& input,
        ::takatori::relation::expression::output_port_type const& output,
        path_capability_options const& options);

} // namespace yugawara::analyzer::details
