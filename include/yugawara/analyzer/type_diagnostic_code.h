#pragma once

#include <iostream>
#include <string_view>
#include <utility>

namespace yugawara::analyzer {

/**
 * @brief represents diagnostic code of type analysis.
 */
enum class type_diagnostic_code {
    /// @brief unknown diagnostic.
    unknown = 0,
    /// @brief input type is not supported in this operation.
    unsupported_type,
    /// @brief input type is not distinguished for the overloaded operations.
    ambiguous_type,
    /// @brief the set of type input is inconsistent for this operation.
    inconsistent_type,
    /// @brief the referring variable is not resolved.
    unresolved_variable,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(type_diagnostic_code value) noexcept {
    using namespace std::string_view_literals;
    using kind = type_diagnostic_code;
    switch (value) {
        case kind::unknown: return "unknown"sv;
        case kind::unsupported_type: return "unsupported_type"sv;
        case kind::ambiguous_type: return "ambiguous_type"sv;
        case kind::inconsistent_type: return "inconsistent_type"sv;
        case kind::unresolved_variable: return "unresolved_variable"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, type_diagnostic_code value) noexcept {
    return out << to_string_view(value);
}

} // namespace yugawara::analyzer
