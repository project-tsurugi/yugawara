#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <cstdlib>

namespace yugawara::aggregate {

/**
 * @brief represents either set (`DISTINCT`) or multi-set (`ALL`) operation.
 */
enum class set_quantifier {
    /// @brief treat the target relation/column as multi-set.
    all,
    /// @brief treat the target relation/column as set.
    distinct,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(set_quantifier value) noexcept {
    using namespace std::string_view_literals;
    using kind = set_quantifier;
    switch (value) {
        case kind::all: return "all"sv;
        case kind::distinct: return "distinct"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, set_quantifier value) {
    return out << to_string_view(value);
}

} // namespace yugawara::aggregate
