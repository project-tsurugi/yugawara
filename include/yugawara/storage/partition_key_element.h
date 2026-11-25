#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <yugawara/storage/column.h>
#include <yugawara/storage/partition/prefix_extractor.h>

namespace yugawara::storage {

/**
 * @brief the partition key element of indices.
 * @details This corresponds to one of the index key columns, at the same position in index::keys().
 *      If this element represents a virtual column, the element value is computed from that index key column
 *      by prefix_extractor().
 * @see index::partition_key_columns()
 */
class partition_key_element {
public:
    /// @brief the column name type.
    using name_type = std::string;

    /// @brief the prefix extractor type.
    using prefix_extractor_type = partition::prefix_extractor;

    /**
     * @brief creates a new instance.
     * @param column the key column, or the source column if this element represents a virtual column
     * @param column_name the name of the key column
     * @param prefix_extractor the prefix extractor to compute this element from the source column,
     *      or empty if this element uses the whole column value
     */
    partition_key_element(
            class column const& column,
            name_type column_name,
            prefix_extractor_type prefix_extractor = {}) noexcept;

    /**
     * @brief creates a new instance which uses the whole column value.
     * @details The name of this element is same as the name of the given column.
     * @param column the key column
     */
    explicit partition_key_element(class column const& column);

    /**
     * @brief returns the key column.
     * @return the key column
     * @attention If this element represents a virtual column, this returns the source column of the virtual column.
     */
    [[nodiscard]] class column const& column() const noexcept;

    /**
     * @brief returns the name of the key column.
     * @details This name may be different from the name of column(), if this element represents a virtual column.
     * @return the name of the key column
     */
    [[nodiscard]] std::string_view column_name() const noexcept;

    /**
     * @brief returns the prefix extractor to compute this element from the source column value.
     * @return the prefix extractor
     * @return empty prefix extractor (prefix_extractor_kind::nothing) if this element uses the whole column value
     */
    [[nodiscard]] prefix_extractor_type const& prefix_extractor() const noexcept;

private:
    class column const* column_;
    name_type column_name_;
    prefix_extractor_type prefix_extractor_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
bool operator==(partition_key_element const& a, partition_key_element const& b) noexcept;

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
bool operator!=(partition_key_element const& a, partition_key_element const& b) noexcept;

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
std::ostream& operator<<(std::ostream& out, partition_key_element const& value);

} // namespace yugawara::storage
