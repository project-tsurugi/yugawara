#pragma once

#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include <cstdlib>

namespace yugawara::storage {

/**
 * @brief represents kind of default column values.
 */
enum class column_value_kind : std::size_t {

    /// @brief no column values, that is, the column must have an explicit default value.
    nothing = 0,

    /// @brief the column should be initialized as `NULL` by default.
    null,

    /// @brief the column should be initialized as the immediate value by default.
    immediate,

    /// @brief the column should be initialized by a sequence by default.
    sequence,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(column_value_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = column_value_kind;
    switch (value) {
        case kind::nothing: return "nothing"sv;
        case kind::null: return "null"sv;
        case kind::immediate: return "immediate"sv;
        case kind::sequence: return "sequence"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, column_value_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::storage
