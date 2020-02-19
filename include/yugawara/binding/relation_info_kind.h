#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <cstdlib>

namespace yugawara::binding {

/**
 * @brief represents kind of external relations from executions.
 * @see relation_info
 */
enum class relation_info_kind {
    /// @brief a table index.
    index,
    /// @brief an upstream exchange.
    exchange,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(relation_info_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = relation_info_kind;
    switch (value) {
        case kind::index: return "index"sv;
        case kind::exchange: return "exchange"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, relation_info_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::binding
