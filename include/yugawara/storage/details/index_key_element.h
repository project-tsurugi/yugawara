#pragma once

#include <iostream>
#include <utility>

#include <takatori/relation/sort_direction.h>

#include "yugawara/storage/column.h"

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
    constexpr class column const& column() const noexcept {
        return *column_;
    }

    /**
     * @brief returns the sort direction of this key element.
     * @return the sort direction
     */
    constexpr sort_direction direction() const noexcept {
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

} // namespace yugawara::storage::details
