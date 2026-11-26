#pragma once

#include <memory>
#include <ostream>

#include <cstddef>

#include "timestamp_unit.h"

#include <takatori/type/data.h>
#include <takatori/value/data.h>
#include <takatori/util/enum_tag.h>

#include <yugawara/type/repository.h>

#include "prefix_extractor_kind.h"

namespace yugawara::storage::partition {

/**
 * @brief truncates timestamp values.
 * @details This is applicable to the `TIMESTAMP` type (without time zone).
 * @see is_applicable()
 * @see compute_type()
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

/**
 * @brief returns whether or not timestamp_truncate is applicable to the given source column type.
 * @details timestamp_truncate is applicable to the source column type,
 *      only if the "parameter application conversion" from the source column type to `TIMESTAMP` (without time zone) is applicable,
 *      as same as the arguments of function calls. Such the conversion never fails.
 * @param source_type the source column type
 * @return true if timestamp_truncate is applicable to the source column type
 * @return false otherwise, or the source column type is erroneous or pending
 * @see type::is_parameter_application_convertible()
 */
[[nodiscard]] bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::timestamp_truncate>,
        ::takatori::type::data const& source_type) noexcept;

/**
 * @brief returns the type of values computed by the given timestamp_truncate.
 * @details The resulting type is:
 *
 *      - `INT` - if the unit is timestamp_unit::year (the year number, as same as `EXTRACT(YEAR FROM ...)`)
 *      - `DATE` - if the unit is timestamp_unit::month or timestamp_unit::day
 *      - `TIMESTAMP` (without time zone) - otherwise
 * @param extractor the prefix extractor
 * @param source_type the source column type
 * @param repo the type repository
 * @return the type of values computed by the prefix extractor
 * @return erroneous type if the prefix extractor is not applicable to the source column type
 * @return pending type if the source column type is erroneous or pending
 * @see is_applicable()
 */
[[nodiscard]] std::shared_ptr<::takatori::type::data const> compute_type(
        timestamp_truncate const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo = ::yugawara::type::default_repository());

/**
 * @brief returns whether or not the given value can be an output of the given timestamp_truncate.
 * @details The value must be:
 *
 *      - timestamp_unit::year - an integral number (an `INT` or `BIGINT` value, or a `DECIMAL` value
 *        without fractional part, but not `REAL` nor `DOUBLE` values)
 *      - timestamp_unit::month - a `DATE` value of the first day of the month
 *      - timestamp_unit::day - a `DATE` value
 *      - timestamp_unit::hour - a `TIMESTAMP` value whose minute, second, and fractional second are all zero
 *      - timestamp_unit::minute - a `TIMESTAMP` value whose second and fractional second are all zero
 *      - timestamp_unit::second - a `TIMESTAMP` value whose fractional second digits under
 *        timestamp_truncate::second_scale() are all zero
 *
 *      For timestamp_unit::hour, timestamp_unit::minute, and timestamp_unit::second, a `DATE` value is also
 *      acceptable, as it represents the beginning of the day.
 *
 *      Null values are always acceptable, and the values of the other types are never acceptable.
 *      This only checks the form of the value, and does not consider the range of the source column type,
 *      that is, this is a necessary condition for that the value is in the image of the prefix extractor.
 * @param extractor the prefix extractor
 * @param value the value
 * @return true if the value can be an output of the prefix extractor, or it is a null value
 * @return false otherwise
 * @see compute_type()
 */
[[nodiscard]] bool is_valid_output(timestamp_truncate const& extractor, ::takatori::value::data const& value) noexcept;

} // namespace yugawara::storage::partition
