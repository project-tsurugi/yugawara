#pragma once

#include <ostream>
#include <string>
#include <string_view>

namespace yugawara {

/**
 * @brief represents diagnostic code of IR compiler.
 */
enum class compiler_code {
    /// @brief unknown diagnostic.
    unknown = 0,
    /// @brief the runtime does not support such target.
    unsupported_feature,
    /// @brief input type is not supported in this operation.
    unsupported_type,
    /// @brief input type is not distinguished for the overloaded operations.
    ambiguous_type,
    /// @brief the set of type input is inconsistent for this operation.
    inconsistent_type,
    /// @brief the referring variable is not resolved.
    unresolved_variable,
    /// @brief the number of values is wrong.
    inconsistent_elements,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(compiler_code value) noexcept {
    using namespace std::string_view_literals;
    using kind = compiler_code;
    switch (value) {
        case kind::unknown: return "unknown"sv;
        case kind::unsupported_feature: return "unsupported_feature"sv;
        case kind::unsupported_type: return "unsupported_type"sv;
        case kind::ambiguous_type: return "ambiguous_type"sv;
        case kind::inconsistent_type: return "inconsistent_type"sv;
        case kind::unresolved_variable: return "unresolved_variable"sv;
        case kind::inconsistent_elements: return "inconsistent_elements"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, compiler_code value) {
    return out << to_string_view(value);
}

} // namespace yugawara
