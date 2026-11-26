#pragma once

#include <ostream>
#include <string_view>

#include <cstdlib>

namespace yugawara::storage::partition {

/**
 * @brief represents a unit to truncate date values.
 */
enum class date_unit {
    /// @brief extracts the year number (as same as `EXTRACT(YEAR FROM ...)`).
    year,
    /// @brief truncates to the first day of the month.
    month,
    /// @brief truncates to the day (does nothing for date values).
    day,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(date_unit value) noexcept {
    using namespace std::string_view_literals;
    using kind = date_unit;
    switch (value) {
        case kind::year: return "year"sv;
        case kind::month: return "month"sv;
        case kind::day: return "day"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, date_unit value) {
    return out << to_string_view(value);
}

} // namespace yugawara::storage::partition
