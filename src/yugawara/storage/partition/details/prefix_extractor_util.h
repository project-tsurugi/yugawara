#pragma once

#include <string_view>

#include <cstddef>
#include <cstdint>

#include <decimal.hh>

#include <takatori/type/data.h>
#include <takatori/type/extension.h>

#include <takatori/value/data.h>
#include <takatori/value/date.h>
#include <takatori/value/decimal.h>
#include <takatori/value/time_point.h>

#include <takatori/datetime/time_point.h>

#include <takatori/util/downcast.h>

#include <yugawara/type/conversion.h>
#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

namespace yugawara::storage::partition::details {

/**
 * @brief returns whether or not the parameter application conversion from the given type to the parameter type is applicable.
 * @param type the source type
 * @param parameter the parameter type
 * @return true if it is applicable
 * @return false otherwise, or it is unknown
 */
inline bool is_parameter_applicable(::takatori::type::data const& type, ::takatori::type::data const& parameter) noexcept {
    return ::yugawara::type::is_parameter_application_convertible(type, parameter) == util::ternary::yes;
}

/**
 * @brief returns whether or not the given type is erroneous or pending.
 * @param type the target type
 * @return true if it is erroneous or pending
 * @return false otherwise
 */
inline bool is_stop_type(::takatori::type::data const& type) noexcept {
    if (type.kind() == ::takatori::type::extension::tag) {
        auto&& ext = ::takatori::util::unsafe_downcast<::takatori::type::extension>(type);
        return extension::type::error::is_instance(ext)
            || extension::type::pending::is_instance(ext);
    }
    return false;
}

/**
 * @brief returns whether or not the given value is a null value.
 * @param value the target value
 * @return true if it is a null value
 * @return false otherwise
 */
inline bool is_null_value(::takatori::value::data const& value) noexcept {
    return value.kind() == ::takatori::value::value_kind::unknown;
}

/**
 * @brief returns the number of characters (Unicode code points) in the given UTF-8 string.
 * @param string the target string
 * @return the number of characters, not including the continuation bytes
 */
inline std::size_t count_characters(std::string_view string) noexcept {
    std::size_t result = 0;
    for (char c : string) {
        if ((static_cast<unsigned char>(c) & 0xc0U) != 0x80U) {
            ++result;
        }
    }
    return result;
}

/**
 * @brief returns whether or not the given exact number value has no non-zero digits under the given scale.
 * @param value the target value
 * @param scale the number of fractional digits
 * @return true if the value is an `INT`, `BIGINT`, or `DECIMAL` value, and its digits under the scale are all zero
 * @return false otherwise
 */
inline bool is_exact_number_with_scale(::takatori::value::data const& value, std::size_t scale) noexcept {
    using kind = ::takatori::value::value_kind;
    switch (value.kind()) {
        case kind::int4:
        case kind::int8:
            return true;
        case kind::decimal:
            break;
        default:
            return false;
    }
    ::decimal::Decimal decimal {
            static_cast<::decimal::Decimal>(::takatori::util::unsafe_downcast<::takatori::value::decimal>(value).get()),
    };
    // NOTE: this only inspects the representation, so that it never depends on ::decimal::context
    auto const* entity = decimal.getconst();
    if (mpd_iszero(entity) != 0) {
        return true;
    }
    // the value is coefficient * 10^exponent, and the trailing zeros of the coefficient are not fractional digits
    auto exponent = static_cast<std::int64_t>(entity->exp) + static_cast<std::int64_t>(mpd_trail_zeros(entity));
    if (exponent >= 0) {
        return true;
    }
    return static_cast<std::uint64_t>(-exponent) <= scale;
}

/**
 * @brief represents the precision of dates.
 */
enum class date_precision {
    /// @brief the day must be the first day of the month.
    month,
    /// @brief any days.
    day,
};

/**
 * @brief returns whether or not the given value is a `DATE` value with the given precision.
 * @param value the target value
 * @param precision the precision
 * @return true if the value is a `DATE` value, and satisfies the precision
 * @return false otherwise
 */
inline bool is_date_value(::takatori::value::data const& value, date_precision precision) noexcept {
    if (value.kind() != ::takatori::value::value_kind::date) {
        return false;
    }
    switch (precision) {
        case date_precision::month:
            return ::takatori::util::unsafe_downcast<::takatori::value::date>(value).get().day() == 1;
        case date_precision::day:
            return true;
    }
    return false;
}

/**
 * @brief represents the precision of time points.
 */
enum class time_point_precision {
    /// @brief minutes and the smaller fields must be zero.
    hour,
    /// @brief seconds and the smaller fields must be zero.
    minute,
    /// @brief the fractional second digits under the scale must be zero.
    second,
};

/**
 * @brief returns whether or not the given value is a `TIMESTAMP` value with the given precision.
 * @details A `DATE` value is also acceptable, as it represents the beginning of the day.
 * @param value the target value
 * @param precision the precision
 * @param second_scale the number of fractional second digits, only for time_point_precision::second
 * @return true if the value satisfies the precision
 * @return false otherwise
 */
inline bool is_time_point_value(
        ::takatori::value::data const& value,
        time_point_precision precision,
        std::size_t second_scale = 0) noexcept {
    using kind = ::takatori::value::value_kind;
    // NOTE: this does not decide whether DATE values are comparable with TIMESTAMP values,
    // it is the responsibility of the type conversion rules
    if (value.kind() == kind::date) {
        return true;
    }
    if (value.kind() != kind::time_point) {
        return false;
    }
    auto time = ::takatori::util::unsafe_downcast<::takatori::value::time_point>(value).get().time();
    auto subsecond = time.subsecond().count();
    switch (precision) {
        case time_point_precision::hour:
            return time.minute() == 0 && time.second() == 0 && subsecond == 0;
        case time_point_precision::minute:
            return time.second() == 0 && subsecond == 0;
        case time_point_precision::second:
            break;
    }
    // the subsecond value must be a multiple of (one second / 10^second_scale) in its unit
    constexpr std::uint64_t radix = 10;
    std::uint64_t unit = ::takatori::datetime::time_point::subsecond_unit::period::den;
    for (std::size_t i = 0; i < second_scale && unit % radix == 0; ++i) {
        unit /= radix;
    }
    return subsecond % unit == 0;
}

} // namespace yugawara::storage::partition::details
