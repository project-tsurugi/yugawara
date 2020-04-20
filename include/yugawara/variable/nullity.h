#pragma once

#include <iostream>
#include <string_view>

namespace yugawara::variable {

/**
 * @brief nullity of variables.
 * @see ::yugawara::variable::nullable
 */
class nullity {
public:
    /**
     * @brief creates a new instance.
     * @param nullable whether or not the variable is nullable
     */
    explicit constexpr nullity(bool nullable) noexcept
        : nullable_(nullable)
    {}


    /**
     * @brief returns whether or not the variable is nullable.
     * @return true if it is nullable
     * @return false otherwise
     */
    [[nodiscard]] constexpr bool nullable() const noexcept {
        return nullable_;
    }

private:
    bool nullable_;
};

/**
 * @brief returns the negation of this nullity.
 * @param value the target nullity
 * @return the negation
 */
inline constexpr nullity operator~(nullity value) noexcept {
    return nullity { !value.nullable() };
}

/**
 * @brief returns the conjunction (AND) of the two nullity.
 * @param a the first nullity
 * @param b the second nullity
 * @return nullable if the both is nullable
 * @return not nullable otherwise
 */
inline constexpr nullity operator&(nullity a, nullity b) noexcept {
    return nullity { a.nullable() && b.nullable() };
}

/**
 * @brief returns the disjunction (OR) of the two nullity.
 * @param a the first nullity
 * @param b the second nullity
 * @return not nullable if the both is not nullable
 * @return nullable otherwise
 */
inline constexpr nullity operator|(nullity a, nullity b) noexcept {
    return nullity { a.nullable() || b.nullable() };
}

/**
 * @brief returns whether or not the two nullity are equivalent.
 * @param a the first nullity
 * @param b the second nullity
 * @return true if a = b
 * @return false otherwise
 */
inline constexpr bool operator==(nullity a, nullity b) noexcept {
    return a.nullable() == b.nullable();
}

/**
 * @brief returns whether or not the two nullity are different.
 * @param a the first nullity
 * @param b the second nullity
 * @return true if a != b
 * @return false otherwise
 */
inline constexpr bool operator!=(nullity a, nullity b) noexcept {
    return !(a == b);
}

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(nullity value) noexcept {
    using namespace std::string_view_literals;
    if (value.nullable()) {
        return "nullable"sv;
    }
    return "~nullable"sv;
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, nullity value) {
    return out << to_string_view(value);
}

/**
 * @brief represents "nullable".
 * @note to obtain "not nullable", please use @c ~nullable
 */
inline constexpr nullity nullable { true };

} // namespace yugawara::variable
