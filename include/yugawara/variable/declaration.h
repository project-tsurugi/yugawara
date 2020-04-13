#pragma once

#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <takatori/type/data.h>
#include <takatori/value/data.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/rvalue_ptr.h>

#include "criteria.h"

namespace yugawara::variable {

/**
 * @brief represents an external variable declaration.
 */
class declaration {
public:
    /// @brief the declaration name type.
    using name_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;

    /// @brief the smart pointer of variable type.
    using type_pointer = std::shared_ptr<takatori::type::data const>;

    /// @brief the smart pointer of constant variables.
    using value_pointer = std::shared_ptr<takatori::value::data const>;

    /**
     * @brief creates a new object.
     * @param name the variable name
     * @param type the variable type, or empty if it is not resolved
     * @param criteria the variable criteria
     */
    explicit declaration(
            name_type name,
            type_pointer type,
            class criteria criteria) noexcept;

    /**
     * @brief creates a new object.
     * @param name the variable name
     * @param type the variable type, or empty if it is not resolved
     * @param criteria the variable criteria
     * @attention this may take copy of arguments
     */
    declaration( // NOLINT
            std::string_view name,
            takatori::util::rvalue_ptr<takatori::type::data> type = {},
            class criteria criteria = nullable);

    /**
     * @brief returns whether or not this variable has been resolved.
     * @return true if this is resolved
     * @return false otherwise
     */
    [[nodiscard]] bool is_resolved() const noexcept;

    /// @copydoc is_resolved()
    [[nodiscard]] explicit operator bool() const noexcept;

    /**
     * @brief returns the variable name.
     * @return the variable name
     */
    [[nodiscard]] std::string_view name() const noexcept;

    /**
     * @brief sets the variable name.
     * @param name the variable name
     * @return this
     */
    declaration& name(name_type name) noexcept;

    /**
     * @brief returns the variable type.
     * @return the type
     * @return unresolved type if the variable is not yet resolved
     */
    [[nodiscard]] takatori::type::data const& type() const noexcept;

    /**
     * @brief returns the variable type as its shared pointer.
     * @return the type
     * @return unresolved type if the variable is not yet resolved
     */
    [[nodiscard]] type_pointer const& shared_type() const noexcept;

    /**
     * @brief sets the variable type.
     * @param type the type
     * @return this
     */
    declaration& type(type_pointer type) noexcept;

    /**
     * @brief returns the criteria of this variable.
     * @details this includes nullity, etc.
     * @return the criteria
     */
    [[nodiscard]] class criteria& criteria() noexcept;

    /// @copydoc criteria()
    [[nodiscard]] class criteria const& criteria() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, declaration const& value);

private:
    name_type name_;
    type_pointer type_;
    value_pointer value_;
    class criteria criteria_; 
};

} // namespace yugawara::variable
