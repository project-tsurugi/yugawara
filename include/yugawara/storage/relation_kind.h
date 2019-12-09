#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <cstdlib>

namespace yugawara::storage {

/**
 * @brief represents kind of external relations.
 */
enum class relation_kind {
    /// @brief a table.
    table,
    /// @brief a view.
    view,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(relation_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = relation_kind;
    switch (value) {
        case kind::table: return "table"sv;
        case kind::view: return "view"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, relation_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::storage
