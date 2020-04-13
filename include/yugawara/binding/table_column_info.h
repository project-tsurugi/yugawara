#pragma once

#include <yugawara/storage/column.h>

#include "variable_info.h"
#include "variable_info_kind.h"

namespace yugawara::binding {

/**
 * @brief refers existing table columns.
 */
class table_column_info : public variable_info {
public:
    /// @brief the variable kind.
    static constexpr variable_info_kind tag = variable_info_kind::table_column;

    /**
     * @brief creates a new object.
     * @param column the target column
     */
    explicit table_column_info(storage::column const& column) noexcept;

    [[nodiscard]] variable_info_kind kind() const noexcept override;

    /**
     * @brief returns the target column.
     * @return the target column
     */
    [[nodiscard]] storage::column const& column() const noexcept;

    /**
     * @brief returns whether or not the two relations are equivalent.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(table_column_info const& a, table_column_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(table_column_info const& a, table_column_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, table_column_info const& value);

protected:
    [[nodiscard]] bool equals(variable_info const& other) const noexcept override;
    [[nodiscard]] std::size_t hash() const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    storage::column const* column_;
};

} // namespace yugawara::binding
