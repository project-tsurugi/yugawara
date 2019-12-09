#pragma once

#include <iostream>

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
    explicit constexpr nullity(bool nullable) noexcept;

    /**
     * @brief returns whether or not the variable is nullable.
     * @return true if it is nullable
     * @return false otherwise
     */
    constexpr bool nullable() const noexcept;

    /**
     * @brief returns the negation of this nullity.
     * @param value the target nullity
     * @return the negation
     */
    friend constexpr nullity operator~(nullity value) noexcept;

    /**
     * @brief returns the conjunction (AND) of the two nullity.
     * @param a the first nullity
     * @param b the second nullity
     * @return nullable if the both is nullable
     * @return not nullable otherwise
     */
    friend constexpr nullity operator&(nullity a, nullity b) noexcept;

    /**
     * @brief returns the disjunction (OR) of the two nullity.
     * @param a the first nullity
     * @param b the second nullity
     * @return not nullable if the both is not nullable
     * @return nullable otherwise
     */
    friend constexpr nullity operator|(nullity a, nullity b) noexcept;

    /**
     * @brief returns whether or not the two nullity are equivalent.
     * @param a the first nullity
     * @param b the second nullity
     * @return true if a = b
     * @return false otherwise
     */
    friend constexpr bool operator==(nullity a, nullity b) noexcept;

    /**
     * @brief returns whether or not the two nullity are different.
     * @param a the first nullity
     * @param b the second nullity
     * @return true if a != b
     * @return false otherwise
     */
    friend constexpr bool operator!=(nullity a, nullity b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, nullity const& value);

private:
    bool nullable_;
};

inline constexpr nullity::nullity(bool nullable) noexcept
    : nullable_(nullable)
{}

constexpr bool nullity::nullable() const noexcept {
    return nullable_;
}

inline constexpr nullity operator~(nullity value) noexcept {
    return nullity { !value.nullable_ };
}

inline constexpr nullity operator&(nullity a, nullity b) noexcept {
    return nullity { a.nullable_ && b.nullable_ };
}

inline constexpr nullity operator|(nullity a, nullity b) noexcept {
    return nullity { a.nullable_ || b.nullable_ };
}

inline constexpr bool operator==(nullity a, nullity b) noexcept {
    return a.nullable_ == b.nullable_;
}

inline constexpr bool operator!=(nullity a, nullity b) noexcept {
    return !(a == b);
}

inline std::ostream& operator<<(std::ostream& out, nullity const& value) {
    if (value.nullable()) {
        return out << "nullable";
    }
    return out << "~nullable";
}

/**
 * @brief represents "nullable".
 * @note to obtain "not nullable", please use @c ~nullable
 */
inline constexpr nullity nullable { true };

} // namespace yugawara::variable
