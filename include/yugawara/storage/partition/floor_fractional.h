#pragma once

#include <ostream>

#include <cstddef>

namespace yugawara::storage::partition {

/**
 * @brief truncates the fractional digits of the value toward negative infinity.
 * @details This is applicable to the exact numeric types (`TINYINT`, `SMALLINT`, `INT`, `BIGINT`, and `DECIMAL`).
 *      For the integral types, this extracts the value as is.
 * @see is_applicable()
 * @see compute_type()
 */
class floor_fractional {
public:
    /// @brief the size type.
    using size_type = std::size_t;

    /**
     * @brief creates a new instance.
     * @param scale the number of fractional digits to keep
     */
    explicit constexpr floor_fractional(size_type scale) noexcept :
        scale_ { scale }
    {}

    /**
     * @brief returns the number of fractional digits to keep.
     * @return the number of fractional digits to keep
     */
    [[nodiscard]] constexpr size_type scale() const noexcept {
        return scale_;
    }

private:
    size_type scale_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
constexpr bool operator==(floor_fractional const& a, floor_fractional const& b) noexcept {
    return a.scale() == b.scale();
}

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
constexpr bool operator!=(floor_fractional const& a, floor_fractional const& b) noexcept {
    return !(a == b);
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, floor_fractional const& value) {
    return out << "floor_fractional("
               << "scale=" << value.scale() << ")";
}

} // namespace yugawara::storage::partition
