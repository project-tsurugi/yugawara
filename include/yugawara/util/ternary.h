#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include <cstdlib>

namespace yugawara::util {

/**
 * @brief an atom of three-valued logic, consists of @c yes, @c no, and @c unknown.
 * @note you can obtain true/false value of ternary by using ternary(bool_value)
 * @note this is based on Kleene's tree-valued logic, but "unknown" is the minimum value of the three in this
 */
enum class ternary {
    /**
     * @brief a true value.
     */
    yes = true,

    /**
     * @brief a false value.
     */
    no = false,

    /**
     * @brief can not decide either true or false.
     * @details This value may appear when a predicate tries to evaluate erroneous inputs.
     */
    unknown = -1,
};

/**
 * @brief returns the negation (NOT) of the ternary value.
 * @param value the value
 * @return the negation of the value
 */
constexpr ternary operator~(ternary value) noexcept {
    switch (value) {
        case ternary::yes: return ternary::no;
        case ternary::no: return ternary::yes;
        case ternary::unknown: return ternary::unknown;
    }
    std::abort();
}

/**
 * @brief returns the conjunction (AND) of the ternary values.
 * @param a the first value
 * @param b the second value
 * @return the conjunction of the two
 */
constexpr ternary operator&(ternary a, ternary b) noexcept {
    if (a == ternary::yes && b == ternary::yes) return ternary::yes;
    if (a == ternary::unknown || b == ternary::unknown) return ternary::unknown;
    return ternary::no;
}

/**
 * @brief returns the disjunction (OR) of the ternary values.
 * @param a the first value
 * @param b the second value
 * @return the conjunction of the two
 */
constexpr ternary operator|(ternary a, ternary b) noexcept {
    if (a == ternary::yes || b == ternary::yes) return ternary::yes;
    if (a == ternary::unknown || b == ternary::unknown) return ternary::unknown;
    return ternary::no;
}

/**
 * @brief compares between ternary and binary value.
 * @param a the ternary value
 * @param b the binary value
 * @return true if the ternary value is equivalent to the binary value
 * @return false otherwise
 */
constexpr bool operator==(ternary a, bool b) noexcept {
    return a == ternary(b);
}

/**
 * @brief compares between ternary and binary value.
 * @param a the binary value
 * @param b the ternary value
 * @return true if the ternary value is equivalent to the binary value
 * @return false otherwise
 */
constexpr bool operator==(bool a, ternary b) noexcept {
    return ternary(a) == b;
}

/**
 * @brief compares between ternary and binary value.
 * @param a the ternary value
 * @param b the binary value
 * @return true if the ternary value is different to the binary value
 * @return false otherwise
 */
constexpr bool operator!=(ternary a, bool b) noexcept {
    return !(a == b);
}

/**
 * @brief compares between ternary and binary value.
 * @param a the binary value
 * @param b the ternary value
 * @return true if the ternary value is different to the binary value
 * @return false otherwise
 */
constexpr bool operator!=(bool a, ternary b) noexcept {
    return !(a == b);
}

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(ternary value) noexcept {
    using namespace std::string_view_literals;
    switch (value) {
        case ternary::yes: return "yes"sv;
        case ternary::no: return "no"sv;
        case ternary::unknown: return "unknown"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, ternary value) noexcept {
    return out << to_string_view(value);
}

} // namespace yugawara::util
