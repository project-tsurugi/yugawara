#pragma once

#include <iostream>
#include <string_view>
#include <utility>

namespace yugawara::analyzer {

/**
 * @brief represents diagnostic code of type analysis.
 */
enum class variable_resolution_kind : std::size_t {
    /// @brief the variable is not yet resolved.
    unresolved = 0,

    /// @brief the variable type is known but cannot follow up the source element.
    unknown,

    /// @brief the variable is resolved as a scalar expression.
    scalar_expression,
    
    /// @brief the variable is an alias of table column.
    table_column,

    /// @brief the variable is declared externally.
    external,

    /// @brief the variable is a result of function call.
    function_call,

    /// @brief the variable is a result of aggregate function call.
    aggregation,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(variable_resolution_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = variable_resolution_kind;
    switch (value) {
        case kind::unresolved: return "unresolved"sv;
        case kind::unknown: return "unknown"sv;
        case kind::scalar_expression: return "scalar_expression"sv;
        case kind::table_column: return "table_column"sv;
        case kind::external: return "external"sv;
        case kind::function_call: return "function_call"sv;
        case kind::aggregation: return "aggregation"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, variable_resolution_kind value) noexcept {
    return out << to_string_view(value);
}

} // namespace yugawara::analyzer

