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
     * @brief the index key can distinguish individual entries.
     */
    unique,
};

/**
 * @brief a set of index_feature.
 */
using index_feature_set = ::takatori::util::enum_set<
        index_feature,
        index_feature::primary,
        index_feature::unique>;

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
