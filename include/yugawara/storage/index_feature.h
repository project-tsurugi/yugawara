#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdlib>

#include <takatori/util/enum_set.h>

namespace yugawara::storage {

/**
 * @brief represents features of indices.
 */
enum class index_feature {
    /**
     * @brief the target is just the origin table itself (primary index).
     * @details each table must have just one primary index, and its index key is called primary key.
     */
    primary = 0,

    /**
     * @brief enable to find entries by their keys.
     * @note If this feature is disabled, the index may not have available keys.
     */
    find,

    /**
     * @brief enable 1-dimensional key range scan.
     * @details This implies the entries on the index are sorted by their index key.
     * @note You can perform full scan even if this feature is disabled.
     */
    scan,

    /**
     * @brief the index key must distinguish individual entries.
     */
    unique,

    /**
     * @brief the index key must be unique in the target index.
     * @note `unique` and `unique_constraint` have different requirements:
     *   * `unique` - AS A RESULT, the index key is always always unique
     *   * `unique_constraint` - if the index key is not unique, transaction should be failed until its end:
     *          it allows duplicated keys while transaction is running
     */
    unique_constraint,
};

/**
 * @brief a set of index_feature.
 */
using index_feature_set = ::takatori::util::enum_set<
        index_feature,
        index_feature::primary,
        index_feature::unique_constraint>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(index_feature value) noexcept {
    using namespace std::string_view_literals;
    using kind = index_feature;
    switch (value) {
        case kind::primary: return "primary"sv;
        case kind::find: return "find"sv;
        case kind::scan: return "scan"sv;
        case kind::unique: return "unique"sv;
        case kind::unique_constraint: return "unique_constraint"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, index_feature value) {
    return out << to_string_view(value);
}

} // namespace yugawara::storage
