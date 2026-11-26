#pragma once

#include <memory>
#include <ostream>

#include <cstddef>

#include <takatori/type/data.h>
#include <takatori/util/enum_tag.h>

#include <yugawara/type/repository.h>

#include "prefix_extractor_kind.h"

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

/**
 * @brief returns whether or not floor_fractional is applicable to the given source column type.
 * @details floor_fractional is applicable to the source column type,
 *      only if the "parameter application conversion" from the source column type to `DECIMAL(*, *)` is applicable,
 *      as same as the arguments of function calls.
 * @param source_type the source column type
 * @return true if floor_fractional is applicable to the source column type
 * @return false otherwise, or the source column type is erroneous or pending
 * @see type::is_parameter_application_convertible()
 */
[[nodiscard]] bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::floor_fractional>,
        ::takatori::type::data const& source_type) noexcept;

/**
 * @brief returns the type of values computed by the given floor_fractional.
 * @details The resulting type is:
 *
 *      - `INT` - if the source column type is `TINYINT`, `SMALLINT`, or `INT`
 *      - `BIGINT` - if the source column type is `BIGINT`
 *      - `DECIMAL(*, *)` - if the source column type is `DECIMAL`
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

} // namespace yugawara::storage::partition
