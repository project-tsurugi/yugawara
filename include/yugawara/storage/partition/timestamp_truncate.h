#pragma once

#include <ostream>

#include <cstddef>

#include "timestamp_unit.h"

namespace yugawara::storage::partition {

/**
 * @brief truncates timestamp values.
 * @details This is applicable to the `TIMESTAMP` type (without time zone).
 */
class timestamp_truncate {
public:
    /// @brief the unit type.
    using unit_type = timestamp_unit;

    /// @brief the size type.
    using size_type = std::size_t;

    /**
     * @brief creates a new instance.
     * @param unit the unit to truncate
     * @param second_scale the number of fractional digits of seconds to keep, only used if the unit is timestamp_unit::second
     */
    explicit constexpr timestamp_truncate(unit_type unit, size_type second_scale = 0) noexcept :
        unit_ { unit },
        second_scale_ { second_scale }
    {}

    /**
     * @brief returns the unit to truncate.
     * @return the unit to truncate
     */
    [[nodiscard]] constexpr unit_type unit() const noexcept {
        return unit_;
    }

    /**
     * @brief returns the number of fractional digits of seconds to keep (only used if the unit is timestamp_unit::second).
     * @details If this is greater than the number of fractional digits of seconds that the timestamp type can hold,
     *      the fractional seconds are kept as is (not truncated).
     * @return the number of fractional digits of seconds to keep (only used if the unit is timestamp_unit::second)
     */
    [[nodiscard]] constexpr size_type second_scale() const noexcept {
        return second_scale_;
    }

private:
    unit_type unit_;
    size_type second_scale_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
constexpr bool operator==(timestamp_truncate const& a, timestamp_truncate const& b) noexcept {
    return a.unit() == b.unit()
        && a.second_scale() == b.second_scale();
}

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
constexpr bool operator!=(timestamp_truncate const& a, timestamp_truncate const& b) noexcept {
    return !(a == b);
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, timestamp_truncate const& value) {
    return out << "timestamp_truncate("
               << "unit=" << value.unit() << ", "
               << "second_scale=" << value.second_scale() << ")";
}

} // namespace yugawara::storage::partition
