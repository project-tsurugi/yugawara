#pragma once

#include <memory>
#include <ostream>

#include <cstdint>

#include <takatori/type/data.h>
#include <takatori/util/enum_tag.h>

#include <yugawara/type/repository.h>

#include "prefix_extractor_kind.h"

namespace yugawara::storage::partition {

/**
 * @brief extracts the floor of the quotient of the value divided by a divisor.
 * @details This is applicable to the exact numeric types (`TINYINT`, `SMALLINT`, `INT`, `BIGINT`, and `DECIMAL`).
 * @see is_applicable()
 * @see compute_type()
 */
class floor_division {
public:
    /// @brief the value type.
    using value_type = std::int64_t;

    /**
     * @brief creates a new instance.
     * @param divisor the divisor, must be positive
     * @attention this does not validate the divisor
     */
    explicit constexpr floor_division(value_type divisor) noexcept :
        divisor_ { divisor }
    {}

    /**
     * @brief returns the divisor.
     * @details The divisor must be positive. This object does not validate it, the producer must satisfy it.
     * @return the divisor
     */
    [[nodiscard]] constexpr value_type divisor() const noexcept {
        return divisor_;
    }

private:
    value_type divisor_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
constexpr bool operator==(floor_division const& a, floor_division const& b) noexcept {
    return a.divisor() == b.divisor();
}

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
constexpr bool operator!=(floor_division const& a, floor_division const& b) noexcept {
    return !(a == b);
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, floor_division const& value) {
    return out << "floor_division("
               << "divisor=" << value.divisor() << ")";
}

/**
 * @brief returns whether or not floor_division is applicable to the given source column type.
 * @details floor_division is applicable to the source column type,
 *      only if the "parameter application conversion" from the source column type to `DECIMAL(*, *)` is applicable,
 *      as same as the arguments of function calls. Such the conversion never fails.
 * @param source_type the source column type
 * @return true if floor_division is applicable to the source column type
 * @return false otherwise, or the source column type is erroneous or pending
 * @see type::is_parameter_application_convertible()
 */
[[nodiscard]] bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::floor_division>,
        ::takatori::type::data const& source_type) noexcept;

/**
 * @brief returns the type of values computed by the given floor_division.
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
        floor_division const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo = ::yugawara::type::default_repository());

} // namespace yugawara::storage::partition
