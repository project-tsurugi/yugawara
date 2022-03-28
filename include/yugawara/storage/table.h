#pragma once

#include <initializer_list>
#include <ostream>
#include <optional>
#include <string_view>

#include <takatori/tree/tree_element_vector.h>
#include <takatori/util/clone_tag.h>

#include "relation.h"
#include "column.h"

namespace yugawara::storage {

/**
 * @brief provides table information.
 */
class table : public relation {
public:
    /// @brief the schema definition ID type.
    using definition_id_type = std::size_t;

    /// @brief the simple name type.
    using simple_name_type = std::string;

    /// @brief the column vector type.
    using column_vector_type = takatori::tree::tree_element_vector<column>;

    /// @brief the kind of this type.
    static constexpr inline relation_kind tag = relation_kind::table;

    /**
     * @brief creates a new object.
     * @param definition_id the table definition ID
     * @param simple_name the simple name
     * @param columns the table columns
     */
    explicit table(
            std::optional<definition_id_type> definition_id,
            simple_name_type simple_name,
            takatori::util::reference_vector<column> columns) noexcept;

    /**
     * @brief creates a new object.
     * @param simple_name the simple name
     * @param columns the table columns
     */
    explicit table(
            simple_name_type simple_name,
            takatori::util::reference_vector<column> columns) noexcept;

    /**
     * @brief creates a new object.
     * @details This is designed for DSL (mainly used in testing).
     * @param definition_id the table definition ID
     * @param simple_name the simple name
     * @param columns the table columns
     * @attention this may take copy of arguments
     */
    table(
            std::optional<definition_id_type> definition_id,
            std::string_view simple_name,
            std::initializer_list<column> columns);

    /**
     * @brief creates a new object.
     * @details This is designed for DSL (mainly used in testing).
     * @param simple_name the simple name
     * @param columns the table columns
     * @attention this may take copy of arguments
     */
    table(
            std::string_view simple_name,
            std::initializer_list<column> columns);

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit table(::takatori::util::clone_tag_t, table const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit table(::takatori::util::clone_tag_t, table&& other);

    [[nodiscard]] table* clone() const& override;
    [[nodiscard]] table* clone() && override;

    [[nodiscard]] relation_kind kind() const noexcept override;

    [[nodiscard]] std::string_view simple_name() const noexcept override;

    /**
     * @brief returns the table definition ID.
     * @return the definition ID
     * @return empty if it is not specified
     */
    [[nodiscard]] std::optional<definition_id_type> definition_id() const noexcept;

    /**
     * @brief sets the table definition ID.
     * @param definition_id the definition ID
     * @return this
     */
    table& definition_id(std::optional<definition_id_type> definition_id) noexcept;

    /**
     * @brief sets the simple name of this table.
     * @param simple_name the table simple name
     * @return this
     */
    table& simple_name(simple_name_type simple_name) noexcept;

    /// @copydoc relation::columns()
    [[nodiscard]] column_vector_type& columns() noexcept;

    [[nodiscard]] column_list_view columns() const noexcept override;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, table const& value);

protected:
    std::ostream& print_to(std::ostream& out) const override;

private:
    std::optional<definition_id_type> definition_id_ {};
    simple_name_type simple_name_;
    column_vector_type columns_;
};

/**
 * @brief returns whether or not the two tables are same.
 * @param a the first table
 * @param b the second table
 * @return true if the both are same
 * @return false otherwise
 */
constexpr bool operator==(table const& a, table const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

/**
 * @brief returns whether or not the two tables are different.
 * @param a the first table
 * @param b the second table
 * @return true if the both are different
 * @return false otherwise
 */
constexpr bool operator!=(table const& a, table const& b) noexcept {
    return !(a == b);
}

} // namespace yugawara::storage
