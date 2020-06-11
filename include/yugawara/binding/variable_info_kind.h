#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <cstdlib>

#include <takatori/util/enum_set.h>

namespace yugawara::binding {

/**
 * @brief represents kind of variables.
 * @see variable_info
 */
enum class variable_info_kind {
    /**
     * @brief columns on the table.
     * @details These variables just refer columns on the table.
     */
    table_column,

    /**
     * @brief columns on the exchange step.
     * @details These variables just refer columns on the exchange step.
     */
    exchange_column,

    /**
     * @brief variables defined as function parameters, or local variables in function bodies.
     * @details The frame variables will be declared in the function, and expressions can use them
     *      only if these expressions are in the same function, and these must be declared the before using them.
     */
    frame_variable,

    /**
     * @brief columns on the current relation.
     * @details These columns will be declared in relational operators,
     *      and may use from downstream operators of the declared one.
     */
    stream_variable,

    /**
     * @brief variables defined in scalar expressions.
     * @details For now, only `takatori::scalar::let` can declare the local variables.
     *      These are only available in the declared scalar expressions, and invalidated after the expressions
     *      were evaluated.
     */
    local_variable,

    /**
     * @brief variables supplied from external context.
     * @details The following variables are external:
     *      - system variables
     *      - session variables
     *      - placeholders
     *      The compiler will consider the above variables as "unresolved", and then the runtime environment
     *      must bind the corresponded values.
     *      These variables
     */
    external_variable,
};

/// @brief an enum set of variable_info_kind.
using variable_info_kind_set = ::takatori::util::enum_set<
        variable_info_kind,
        variable_info_kind::table_column,
        variable_info_kind::external_variable>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(variable_info_kind value) noexcept {
    using namespace std::string_view_literals;
    using kind = variable_info_kind;
    switch (value) {
        case kind::table_column: return "table_column"sv;
        case kind::exchange_column: return "exchange_column"sv;
        case kind::external_variable: return "external_variable"sv;
        case kind::frame_variable: return "frame_variable"sv;
        case kind::stream_variable: return "stream_variable"sv;
        case kind::local_variable: return "local_variable"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, variable_info_kind value) {
    return out << to_string_view(value);
}

} // namespace yugawara::binding
