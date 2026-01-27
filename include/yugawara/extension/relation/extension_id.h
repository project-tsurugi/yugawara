#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <takatori/relation/intermediate/extension.h>

namespace yugawara::extension::relation {

/// @brief the minimum extension ID for yugawara.
constexpr ::takatori::relation::intermediate::extension::extension_id_type min_id = 1'000;

/// @brief the maximum extension ID for yugawara.
constexpr ::takatori::relation::intermediate::extension::extension_id_type max_id = 1'999;

/**
 * @brief extension IDs.
 */
enum extension_id : ::takatori::relation::intermediate::extension::extension_id_type {

    /// @brief represents a compile error.
    error_id = min_id + 1,

    /// @brief pseudo subquery expression.
    subquery_id,

    /// @brief the minimum unused ID.
    min_unused_id,
};

/**
 * @brief returns whether the given ID is known compiler extension ID.
 * @param id the extension ID
 * @return true if it is the known extension ID
 * @return false otherwise
 */
constexpr bool is_known_compiler_extension(::takatori::relation::intermediate::extension::extension_id_type id) noexcept {
    return static_cast<::takatori::relation::intermediate::extension::extension_id_type>(error_id) <= id
            && id < static_cast<::takatori::relation::intermediate::extension::extension_id_type>(min_unused_id);
}

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(extension_id value) noexcept {
    using namespace std::string_view_literals;
    switch (value) {
        case error_id: return "error"sv;
        case subquery_id: return "subquery"sv;
        default: return "(unknown extension kind)"sv;
    }
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, extension_id value) {
    return out << to_string_view(value);
}

} // namespace yugawara::extension::relation
