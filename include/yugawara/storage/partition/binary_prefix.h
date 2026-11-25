#pragma once

#include <ostream>

#include <cstddef>

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

} // namespace yugawara::storage::partition
