#pragma once

#include <ostream>
#include <string_view>

#include <cstdlib>

namespace yugawara::storage::partition {

/**
 * @brief represents a unit to truncate timestamp values.
 */
enum class timestamp_unit {
    /// @brief truncates to the beginning of the year.
    year,
    /// @brief truncates to the beginning of the month.
    month,
    /// @brief truncates to the beginning of the day.
    day,
    /// @brief truncates to the beginning of the hour.
    hour,
    /// @brief truncates to the beginning of the minute.
    minute,
    /// @brief truncates to the second, with the fractional digits of timestamp_truncate::second_scale().
    second,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(timestamp_unit value) noexcept {
    using namespace std::string_view_literals;
    using kind = timestamp_unit;
    switch (value) {
        case kind::year: return "year"sv;
        case kind::month: return "month"sv;
        case kind::day: return "day"sv;
        case kind::hour: return "hour"sv;
        case kind::minute: return "minute"sv;
        case kind::second: return "second"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, timestamp_unit value) {
    return out << to_string_view(value);
}

} // namespace yugawara::storage::partition
