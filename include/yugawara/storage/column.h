#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

#include <takatori/type/data.h>
#include <takatori/util/clone_tag.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/reference_list_view.h>
#include <takatori/util/rvalue_ptr.h>

#include <yugawara/variable/criteria.h>

#include "column_value.h"

namespace yugawara::storage {

class relation;

class column;

/**
 * @brief a list view of columns on relation.
 */
using column_list_view = takatori::util::reference_list_view<takatori::util::double_pointer_extractor<column const>>;

/**
 * @brief represents a column on external relations.
 */
class column {
public:
    /// @brief the simple name type.
    using simple_name_type = std::string;

    /**
     * @brief constructs a new object.
     * @param simple_name the simple name
     * @param type the column data type
     * @param criteria the column value criteria
     * @param default_value the column default value
     */
    explicit column(
            simple_name_type simple_name,
            std::shared_ptr<takatori::type::data const> type,
            variable::criteria criteria,
            column_value default_value) noexcept;

    /**
     * @brief constructs a new object.
     * @param simple_name the simple name
     * @param type the column data type
     * @param criteria the column value criteria
     * @param default_value the column default value (nullable)
     * @attention this may take copy of the arguments
     */
    column(
            std::string_view simple_name,
            takatori::type::data&& type,
            variable::criteria criteria = {},
            column_value default_value = {});

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit column(::takatori::util::clone_tag_t, column const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit column(::takatori::util::clone_tag_t, column&& other);

    /**
     * @brief returns a clone of this object.
     * @return the created clone
     */
    [[nodiscard]] column* clone() const&;

    /// @copydoc clone()
    [[nodiscard]] column* clone() &&;

    /**
     * @brief returns the column name.
     * @return the column name
     */
    [[nodiscard]] std::string_view simple_name() const noexcept;

    /**
     * @brief sets the simple name of this column.
     * @param simple_name the column simple name
     * @return this
     */
    column& simple_name(simple_name_type simple_name) noexcept;

    /**
     * @brief returns the column type.
     * @return the column type
     */
    [[nodiscard]] takatori::type::data const& type() const noexcept;

    /**
     * @brief returns the column type.
     * @return the column type
     * @return empty if the type is absent
     */
    [[nodiscard]] takatori::util::optional_ptr<takatori::type::data const> optional_type() const noexcept;

    /**
     * @brief returns the column type for share its object.
     * @return the column type for sharing
     * @return empty if the type is absent
     */
    [[nodiscard]] std::shared_ptr<takatori::type::data const> shared_type() const noexcept;

    /**
     * @brief sets a column type.
     * @param type the column type
     * @return this
     */
    column& type(std::shared_ptr<takatori::type::data const> type) noexcept;

    /**
     * @brief returns the criteria of this column.
     * @details this includes nullity, etc.
     * @return the criteria
     */
    [[nodiscard]] variable::criteria& criteria() noexcept;

    /// @copydoc criteria()
    [[nodiscard]] variable::criteria const& criteria() const noexcept;

    /**
     * @brief returns the default value.
     * @return the default value
     */
    [[nodiscard]] column_value& default_value() noexcept;

    /// @copydoc default_value()
    [[nodiscard]] column_value const& default_value() const noexcept;

    /**
     * @brief returns what declares this column.
     * @return the column declarator
     * @attention undefined behavior if this column is orphaned from the owner relations
     * @see optional_owner()
     */
    [[nodiscard]] relation const& owner() const noexcept;

    /**
     * @brief returns what declares this column.
     * @return the column declarator
     * @return empty if this column is orphaned from the owner relations
     */
    [[nodiscard]] takatori::util::optional_ptr<relation const> optional_owner() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, column const& value);

    /**
     * @brief sets the parent relation.
     * @param parent the parent element
     * @attention developers should not call this function directly
     */
    void parent_element(relation* parent) noexcept;

private:
    simple_name_type simple_name_;
    std::shared_ptr<takatori::type::data const> type_;
    variable::criteria criteria_;
    column_value default_value_;
    relation* owner_ {};
};

/**
 * @brief returns whether or not the two columns are same.
 * @param a the first column
 * @param b the second column
 * @return true if the both are identical
 * @return false otherwise
 */
constexpr bool operator==(column const& a, column const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

/**
 * @brief returns whether or not the two columns are different.
 * @param a the first column
 * @param b the second column
 * @return true if the both are different
 * @return false otherwise
 */
constexpr bool operator!=(column const& a, column const& b) noexcept {
    return !(a == b);
}

} // namespace yugawara::storage
