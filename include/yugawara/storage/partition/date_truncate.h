#pragma once

#include <memory>
#include <ostream>

#include "date_unit.h"

#include <takatori/type/data.h>
#include <takatori/util/enum_tag.h>

#include <yugawara/type/repository.h>

#include "prefix_extractor_kind.h"

namespace yugawara::storage::partition {

/**
 * @brief truncates date values.
 * @details This is applicable to the `DATE` type.
 * @see is_applicable()
 * @see compute_type()
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

/**
 * @brief returns whether or not date_truncate is applicable to the given source column type.
 * @details date_truncate is applicable to the source column type,
 *      only if the "parameter application conversion" from the source column type to `DATE` is applicable,
 *      as same as the arguments of function calls. Such the conversion never fails.
 * @param source_type the source column type
 * @return true if date_truncate is applicable to the source column type
 * @return false otherwise, or the source column type is erroneous or pending
 * @see type::is_parameter_application_convertible()
 */
[[nodiscard]] bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::date_truncate>,
        ::takatori::type::data const& source_type) noexcept;

/**
 * @brief returns the type of values computed by the given date_truncate.
 * @details The resulting type is:
 *
 *      - `INT` - if the unit is date_unit::year (the year number, as same as `EXTRACT(YEAR FROM ...)`)
 *      - `DATE` - otherwise
 * @param extractor the prefix extractor
 * @param source_type the source column type
 * @param repo the type repository
 * @return the type of values computed by the prefix extractor
 * @return erroneous type if the prefix extractor is not applicable to the source column type
 * @return pending type if the source column type is erroneous or pending
 * @see is_applicable()
 */
[[nodiscard]] std::shared_ptr<::takatori::type::data const> compute_type(
        date_truncate const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo = ::yugawara::type::default_repository());

} // namespace yugawara::storage::partition
