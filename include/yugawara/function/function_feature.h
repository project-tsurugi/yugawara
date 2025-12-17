#pragma once

#include <ostream>
#include <string_view>

#include <takatori/util/enum_set.h>

namespace yugawara::function {

/**
 * @brief represents features of indices.
 */
enum class function_feature {
    /**
     * @brief represents scalar functions.
     */
    scalar_function = 0,

    // set_function,

    /**
     * @brief represents table-valued functions.
     * @details the table-valued functions must have a special return type representing its table.
     */
    table_valued_function,
};

/**
 * @brief a set of function_feature.
 */
using function_feature_set = ::takatori::util::enum_set<
        function_feature,
        function_feature::scalar_function,
        function_feature::table_valued_function>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(function_feature value) noexcept {
    using namespace std::string_view_literals;
    using kind = function_feature;
    switch (value) {
        case kind::scalar_function: return "scalar_function"sv;
        case kind::table_valued_function: return "table_valued_function"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, function_feature value) {
    return out << to_string_view(value);
}

} // namespace yugawara::function
