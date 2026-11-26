#pragma once

#include <memory>
#include <ostream>

#include <cstddef>

#include <takatori/type/data.h>
#include <takatori/value/data.h>
#include <takatori/util/enum_tag.h>

#include <yugawara/type/repository.h>

#include "prefix_extractor_kind.h"

namespace yugawara::storage::partition {

/**
 * @brief truncates the fractional digits of the value toward negative infinity.
 * @details This is applicable only to the `DECIMAL` type.
 *      It is not applicable to the integral types, because it does not change their values.
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

/**
 * @brief returns whether or not floor_fractional is applicable to the given source column type.
 * @details floor_fractional is applicable only to the `DECIMAL` type.
 *      Unlike the other prefix extractors, this does not accept the source column types which are convertible to
 *      `DECIMAL(*, *)` by the "parameter application conversion" (e.g. `INT`), because the resulting type must not be
 *      larger than the source column type (see compute_type(prefix_extractor const&, ...)).
 * @param source_type the source column type
 * @return true if floor_fractional is applicable to the source column type
 * @return false otherwise, or the source column type is erroneous or pending
 */
[[nodiscard]] bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::floor_fractional>,
        ::takatori::type::data const& source_type) noexcept;

/**
 * @brief returns the type of values computed by the given floor_fractional.
 * @details The resulting type is `DECIMAL(*, *)`.
 * @param extractor the prefix extractor
 * @param source_type the source column type
 * @param repo the type repository
 * @return the type of values computed by the prefix extractor
 * @return erroneous type if the prefix extractor is not applicable to the source column type
 * @return pending type if the source column type is erroneous or pending
 * @see is_applicable()
 */
[[nodiscard]] std::shared_ptr<::takatori::type::data const> compute_type(
        floor_fractional const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo = ::yugawara::type::default_repository());

/**
 * @brief returns whether or not the given value can be an output of the given floor_fractional.
 * @details The value must be an exact number whose fractional digits under floor_fractional::scale() are all
 *      zero, that is, a `DECIMAL` value of such the form, or an `INT` or `BIGINT` value (they are comparable with
 *      the resulting `DECIMAL` values). Approximate numbers (`REAL` and `DOUBLE`) are never acceptable.
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
[[nodiscard]] bool is_valid_output(floor_fractional const& extractor, ::takatori::value::data const& value) noexcept;

} // namespace yugawara::storage::partition
