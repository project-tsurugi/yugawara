#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdlib>

#include <takatori/util/enum_set.h>

namespace yugawara::analyzer::details {

/**
 * @brief represents features of indices.
 */
enum class index_estimator_result_attribute {
    /**
     * @brief enable find operation.
     * @details the specified search key covers all index key, and can point the target rows with equivalent condition.
     */
    find,

    /**
     * @brief enable range scan operation.
     * @details the specified search key covers one or more columns in the index key.
     */
    range_scan,

    /**
     * @brief the fetch target row is upto one.
     */
    single_row,

    /**
     * @brief index only access.
     * @details if the result has this attribute, the operation can obtain the all requested columns
     *      without table access.
     */
    index_only,

    /**
     * @brief entries on the index were already sorted by the requested key.
     */
    sorted,
};

/**
 * @brief a set of index_estimator_result_attribute.
 */
using index_estimator_result_attribute_set = ::takatori::util::enum_set<
        index_estimator_result_attribute,
        index_estimator_result_attribute::find,
        index_estimator_result_attribute::sorted>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(index_estimator_result_attribute value) noexcept {
    using namespace std::string_view_literals;
    using kind = index_estimator_result_attribute;
    switch (value) {
        case kind::find: return "find"sv;
        case kind::range_scan: return "range_scan"sv;
        case kind::single_row: return "single_row"sv;
        case kind::index_only: return "index_only"sv;
        case kind::sorted: return "sorted"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, index_estimator_result_attribute value) {
    return out << to_string_view(value);
}

} // namespace yugawara::analyzer::details