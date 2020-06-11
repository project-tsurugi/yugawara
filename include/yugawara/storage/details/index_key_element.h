#pragma once

#include <ostream>
#include <utility>

#include <takatori/relation/sort_direction.h>

#include <yugawara/storage/column.h>

namespace yugawara::storage {

using ::takatori::relation::sort_direction;

} // namespace yugawara::storage

namespace yugawara::storage::details {

/**
 * @brief the index key element.
 */
class index_key_element {
public:
    /**
     * @brief creates a new instance.
     * @param column the index key column
     * @param direction the sort direction
     * @note @c direction is ignored if the target index is not sorted
     */
    constexpr index_key_element( // NOLINT
            class column const& column,
            sort_direction direction = sort_direction::ascendant) noexcept
        : column_(std::addressof(column))
        , direction_(direction)
    {}

    /**
     * @brief returns the index key column.
     * @return the key column
     */
    [[nodiscard]] constexpr class column const& column() const noexcept {
        return *column_;
    }

    /**
     * @brief returns the sort direction of this key element.
     * @return the sort direction
     */
    [[nodiscard]] constexpr sort_direction direction() const noexcept {
        return direction_;
    }

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, index_key_element const& value);

private:
    class column const* column_;
    sort_direction direction_;
};

/**
 * @brief returns whether or not the two index key elements are same.
 * @param a the first index
 * @param b the second index
 * @return true if the both are same
 * @return false otherwise
 */
constexpr bool operator==(index_key_element const& a, index_key_element const& b) noexcept {
    return a.column() == b.column()
        && a.direction() == b.direction();
}

/**
 * @brief returns whether or not the two index key elements are different.
 * @param a the first index
 * @param b the second index
 * @return true if the both are different
 * @return false otherwise
 */
constexpr bool operator!=(index_key_element const& a, index_key_element const& b) noexcept {
    return !(a == b);
}

} // namespace yugawara::storage::details
