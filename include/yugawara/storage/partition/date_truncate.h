#pragma once

#include <ostream>

#include "date_unit.h"

namespace yugawara::storage::partition {

/**
 * @brief truncates date values.
 * @details This is applicable to the `DATE` type.
 */
class date_truncate {
public:
    /// @brief the unit type.
    using unit_type = date_unit;

    /**
     * @brief creates a new instance.
     * @param unit the unit to truncate
     */
    explicit constexpr date_truncate(unit_type unit) noexcept :
        unit_ { unit }
    {}

    /**
     * @brief returns the unit to truncate.
     * @return the unit to truncate
     */
    [[nodiscard]] constexpr unit_type unit() const noexcept {
        return unit_;
    }

private:
    unit_type unit_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
constexpr bool operator==(date_truncate const& a, date_truncate const& b) noexcept {
    return a.unit() == b.unit();
}

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
constexpr bool operator!=(date_truncate const& a, date_truncate const& b) noexcept {
    return !(a == b);
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, date_truncate const& value) {
    return out << "date_truncate("
               << "unit=" << value.unit() << ")";
}

} // namespace yugawara::storage::partition
