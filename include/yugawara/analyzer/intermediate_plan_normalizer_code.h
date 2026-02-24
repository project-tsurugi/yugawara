#pragma once

#include <ostream>
#include <string_view>

namespace yugawara::analyzer {

/**
 * @brief represents diagnostic code of type analysis.
 */
enum class intermediate_plan_normalizer_code {
    /// @brief unknown diagnostic.
    unknown = 0,

    /// @brief scalar subquery is not supported here.
    unsupported_scalar_subquery_placement,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(intermediate_plan_normalizer_code value) noexcept {
    using namespace std::string_view_literals;
    using kind = intermediate_plan_normalizer_code;
    switch (value) {
        case kind::unknown: return "unknown"sv;
        case kind::unsupported_scalar_subquery_placement: return "unsupported_scalar_subquery_placement"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, intermediate_plan_normalizer_code value) noexcept {
    return out << to_string_view(value);
}

} // namespace yugawara::analyzer

