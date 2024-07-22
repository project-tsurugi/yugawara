#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdlib>

#include <takatori/util/enum_set.h>

namespace yugawara::storage {

/**
 * @brief represents features of columns.
 */
enum class column_feature {
    /**
     * @brief the target column is defined by the system (NOT explicitly defined by a user).
     * @details users cannot access these columns.
     */
    synthesized = 0,

    /**
     * @brief the target column is hidden from the user.
     * @details these columns are ignored the following operations:
     * - `SELECT *`
     * - `NATURAL JOIN`, and similar column name based matching (e.g. `UNION CORRESPONDING`)
     */
    hidden = 1,

    /**
     * @brief the target column is read-only.
     * @details The read-only columns are created or update only with its default value.
     * @attention The compiler does not treat this feature.
     */
    read_only = 2,
};

/**
 * @brief a set of column_feature.
 */
using column_feature_set = ::takatori::util::enum_set<
        column_feature,
        column_feature::synthesized,
        column_feature::read_only>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(column_feature value) noexcept {
    using namespace std::string_view_literals;
    using kind = column_feature;
    switch (value) {
        case kind::synthesized: return "synthesized"sv;
        case kind::hidden: return "hidden"sv;
        case kind::read_only: return "read_only"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, column_feature value) {
    return out << to_string_view(value);
}

} // namespace yugawara::storage
