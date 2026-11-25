#pragma once

#include <ostream>

#include <cstdint>

namespace yugawara::storage::partition {

/**
 * @brief extracts the floor of the quotient of the value divided by a divisor.
 * @details This is applicable to the exact numeric types (`INT`, `BIGINT`, and `DECIMAL`).
 */
class floor_division {
public:
    /// @brief the value type.
    using value_type = std::int64_t;

    /**
     * @brief creates a new instance.
     * @param divisor the divisor, must be positive
     * @attention this does not validate the divisor
     */
    explicit constexpr floor_division(value_type divisor) noexcept :
        divisor_ { divisor }
    {}

    /**
     * @brief returns the divisor.
     * @details The divisor must be positive. This object does not validate it, the producer must satisfy it.
     * @return the divisor
     */
    [[nodiscard]] constexpr value_type divisor() const noexcept {
        return divisor_;
    }

private:
    value_type divisor_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
constexpr bool operator==(floor_division const& a, floor_division const& b) noexcept {
    return a.divisor() == b.divisor();
}

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
constexpr bool operator!=(floor_division const& a, floor_division const& b) noexcept {
    return !(a == b);
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, floor_division const& value) {
    return out << "floor_division("
               << "divisor=" << value.divisor() << ")";
}

} // namespace yugawara::storage::partition
