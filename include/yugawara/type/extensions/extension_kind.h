#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <takatori/type/extension.h>

namespace yugawara::type::extensions {

/// @brief the minimum extension ID for yugawara.
constexpr ::takatori::type::extension::extension_id_type min_id = 1'000;

/// @brief the maximum extension ID for yugawara.
constexpr ::takatori::type::extension::extension_id_type max_id = 1'999;

/**
 * @brief error kind in type conversion operations.
 */
enum extension_kind : ::takatori::type::extension::extension_id_type {

    /// @brief represents a compile error.
    error_id = min_id + 1,

    /// @brief type computation is pending by unresolved symbols or upstream errors.
    pending_id,

    /// @brief the minimum unused ID.
    min_unused_id,
};

/**
 * @brief returns whether or not the given ID is known compiler extension ID.
 * @param id the extension ID
 * @return true if it is the known extension ID
 * @return false otherwise
 */
inline constexpr bool is_known_compiler_extension(::takatori::type::extension::extension_id_type id) noexcept {
    return static_cast<::takatori::type::extension::extension_id_type>(error_id) <= id
            && id < static_cast<::takatori::type::extension::extension_id_type>(min_unused_id);
}

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr inline std::string_view to_string_view(extension_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = extension_kind;
    switch (value) {
        case kind::error_id: return "error"sv;
        case kind::pending_id: return "pending"sv;
        default: return "(unknown extension kind)"sv;
    }
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, extension_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::type::extensions
