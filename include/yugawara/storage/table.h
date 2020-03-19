#pragma once

#include <initializer_list>
#include <iostream>
#include <string_view>

#include <takatori/tree/tree_element_vector.h>
#include <takatori/util/object_creator.h>

#include "relation.h"
#include "column.h"

namespace yugawara::storage {

/**
 * @brief provides table information.
 */
class table : public relation {
public:
    /// @brief the simple name type.
    using simple_name_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;

    /// @brief the column vector type.
    using column_vector_type = takatori::tree::tree_element_vector<column>;

    /// @brief the kind of this type.
    static constexpr inline relation_kind tag = relation_kind::table;

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
     * @param creator the object creator
     */
    explicit table(table const& other, takatori::util::object_creator creator);

    /**
     * @brief creates a new object.
     * @param other the move source
     * @param creator the object creator
     */
    explicit table(table&& other, takatori::util::object_creator creator);

    table* clone(takatori::util::object_creator creator) const& override;
    table* clone(takatori::util::object_creator creator) && override;

    relation_kind kind() const noexcept override;

    std::string_view simple_name() const noexcept override;

    /**
     * @brief sets the simple name of this table.
     * @param simple_name the table simple name
     * @return this
     */
    table& simple_name(simple_name_type simple_name) noexcept;

    column_list_view columns() const noexcept override;

    /// @copydoc relation::columns()
    column_vector_type& columns() noexcept;

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
    simple_name_type simple_name_;
    column_vector_type columns_;
};

} // namespace yugawara::storage