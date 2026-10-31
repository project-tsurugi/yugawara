#pragma once

#include <ostream>
#include <string_view>

#include <cstdlib>

#include <takatori/relation/expression.h>

#include <takatori/util/enum_set.h>

namespace yugawara::analyzer::details {

/**
 * @brief represents a capability of the path from an input port to an output port of the same operator.
 * @details The capabilities are used to decide whether `limit` operators can be placed to upstream of the operator.
 */
enum class path_capability {
    /**
     * @brief the order of output rows is consistent with the order of input rows.
     * @details The output rows derived from the same input row may be contiguous.
     */
    preserves_order,

    /// @brief each input row becomes one or more output rows, with keeping the values of its columns.
    at_least_one,

    /// @brief each input row becomes exactly one output row, with keeping the values of its columns.
    exactly_one,
};

/**
 * @brief a set of path_capability.
 */
using path_capability_set = ::takatori::util::enum_set<
        path_capability,
        path_capability::preserves_order,
        path_capability::exactly_one>;

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
        case kind::exactly_one: return "exactly_one"sv;
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
 * @brief returns the capabilities of the path from the input port to the output port.
 * @details This only knows the operators which are required to place `limit` operators to upstream,
 *      and returns an empty set for the other operators.
 *      Currently, this only returns the capabilities of `project`.
 * @param input the input port of the operator
 * @param output the output port of the same operator
 * @return the capabilities of the path
 * @return empty set if the operator is not known, or the ports do not belong to the same operator
 */
[[nodiscard]] path_capability_set find_path_capabilities(
        ::takatori::relation::expression::input_port_type const& input,
        ::takatori::relation::expression::output_port_type const& output);

} // namespace yugawara::analyzer::details
