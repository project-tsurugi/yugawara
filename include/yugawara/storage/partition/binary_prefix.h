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
 * @brief extracts a prefix of binary strings.
 * @details This is applicable to the binary string types (`BINARY` and `VARBINARY`).
 * @see is_applicable()
 * @see compute_type()
 */
class binary_prefix {
public:
    /// @brief the size type.
    using size_type = std::size_t;

    /**
     * @brief creates a new instance.
     * @param length the number of octets to extract, must be positive
     * @attention this does not validate the length
     */
    explicit constexpr binary_prefix(size_type length) noexcept :
        length_ { length }
    {}

    /**
     * @brief returns the number of octets to extract.
     * @details The length must be positive. This object does not validate it, the producer must satisfy it.
     * @return the number of octets to extract
     */
    [[nodiscard]] constexpr size_type length() const noexcept {
        return length_;
    }

private:
    size_type length_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
constexpr bool operator==(binary_prefix const& a, binary_prefix const& b) noexcept {
    return a.length() == b.length();
}

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
constexpr bool operator!=(binary_prefix const& a, binary_prefix const& b) noexcept {
    return !(a == b);
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, binary_prefix const& value) {
    return out << "binary_prefix("
               << "length=" << value.length() << ")";
}

/**
 * @brief returns whether or not binary_prefix is applicable to the given source column type.
 * @details binary_prefix is applicable to the source column type,
 *      only if the "parameter application conversion" from the source column type to `VARBINARY(*)` is applicable,
 *      as same as the arguments of function calls. Such the conversion never fails.
 * @param source_type the source column type
 * @return true if binary_prefix is applicable to the source column type
 * @return false otherwise, or the source column type is erroneous or pending
 * @see type::is_parameter_application_convertible()
 */
[[nodiscard]] bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::binary_prefix>,
        ::takatori::type::data const& source_type) noexcept;

/**
 * @brief returns the type of values computed by the given binary_prefix.
 * @details The resulting type is `VARBINARY(*)`.
 * @param extractor the prefix extractor
 * @param source_type the source column type
 * @param repo the type repository
 * @return the type of values computed by the prefix extractor
 * @return erroneous type if the prefix extractor is not applicable to the source column type
 * @return pending type if the source column type is erroneous or pending
 * @see is_applicable()
 */
[[nodiscard]] std::shared_ptr<::takatori::type::data const> compute_type(
        binary_prefix const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo = ::yugawara::type::default_repository());

/**
 * @brief returns whether or not the given value can be an output of the given binary_prefix.
 * @details The value must be an octet string whose length is less than or equal to binary_prefix::length()
 *      in octets.
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
[[nodiscard]] bool is_valid_output(binary_prefix const& extractor, ::takatori::value::data const& value) noexcept;

} // namespace yugawara::storage::partition
