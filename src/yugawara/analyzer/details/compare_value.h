#pragma once

#include <cstdlib>

#include <string_view>

#include <takatori/value/data.h>

namespace yugawara::analyzer::details {

/**
 * @brief comparison result between two values.
 */
enum class compare_result {
    /**
     * @brief comparison result is undefined (not comparable).
     */
    undefined,

    /**
     * @brief the two values are equivalent.
     */
    equal,

    /**
     * @brief the first value is less than the second value.
     */
    less,

    /**
     * @brief the first value is greater than the second value.
     */
    greater,
};

/**
 * @brief returns the negated comparison result.
 * @param value the target value
 * @return the negated comparison result
 */
constexpr compare_result operator~(compare_result value) noexcept {
    using kind = compare_result;
    switch (value) {
        case kind::undefined: return kind::undefined;
        case kind::equal: return kind::equal;
        case kind::less: return kind::greater;
        case kind::greater: return kind::less;
    }
    std::abort();
}

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(compare_result value) noexcept {
    using namespace std::string_view_literals;
    using kind = compare_result;
    switch (value) {
        case kind::undefined: return "?"sv;
        case kind::equal: return "="sv;
        case kind::less: return "<"sv;
        case kind::greater: return ">"sv;
    }
    std::abort();
}

/**
 * @brief compares two values.
 * @param left the left value
 * @param right the right value
 * @return the comparison result
 */
[[nodiscard]] compare_result compare(::takatori::value::data const& left, ::takatori::value::data const& right);

} // namespace yugawara::analyzer::details
