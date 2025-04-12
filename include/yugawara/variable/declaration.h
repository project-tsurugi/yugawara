#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

#include <takatori/type/data.h>
#include <takatori/value/data.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/rvalue_ptr.h>

#include "criteria.h"

namespace yugawara::variable {

/**
 * @brief represents an external variable declaration.
 */
class declaration {
public:
    /// @brief the schema definition ID type.
    using definition_id_type = std::size_t;

    /// @brief the declaration name type.
    using name_type = std::string;

    /// @brief the smart pointer of variable type.
    using type_pointer = std::shared_ptr<takatori::type::data const>;

    /// @brief the smart pointer of constant variables.
    using value_pointer = std::shared_ptr<takatori::value::data const>;

    /// @brief the variable description type.
    using description_type = std::string;

    /**
     * @brief creates a new object.
     * @param definition_id the variable definition ID
     * @param name the variable name
     * @param type the variable type, or empty if it is not resolved
     * @param criteria the variable criteria
     * @param description the optional description of this element
     */
    explicit declaration(
            std::optional<definition_id_type> definition_id,
            name_type name,
            type_pointer type,
            class criteria criteria,
            description_type description = {}) noexcept;

    /**
     * @brief creates a new object.
     * @param name the variable name
     * @param type the variable type, or empty if it is not resolved
     * @param criteria the variable criteria
     * @param description the optional description of this element
     */
    explicit declaration(
            name_type name,
            type_pointer type,
            class criteria criteria,
            description_type description = {}) noexcept;

    /**
     * @brief creates a new object.
     * @param name the variable name
     * @param type the variable type, or empty if it is not resolved
     * @param criteria the variable criteria
     * @param description the optional description of this element
     * @attention this may take copy of arguments
     */
    declaration( // NOLINT
            std::string_view name,
            takatori::util::rvalue_ptr<takatori::type::data> type = {},
            class criteria criteria = nullable,
            description_type description = {});

    /**
     * @brief returns whether or not this variable has been resolved.
     * @return true if this is resolved
     * @return false otherwise
     */
    [[nodiscard]] bool is_resolved() const noexcept;

    /// @copydoc is_resolved()
    [[nodiscard]] explicit operator bool() const noexcept;

    /**
     * @brief returns the variable definition ID.
     * @return the definition ID
     * @return empty if it is not specified
     */
    [[nodiscard]] std::optional<definition_id_type> definition_id() const noexcept;

    /**
     * @brief sets the variable definition ID.
     * @param definition_id the definition ID
     * @return this
     */
    declaration& definition_id(std::optional<definition_id_type> definition_id) noexcept;

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

    /// @copydoc type()
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::type::data const> optional_type() const noexcept;

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
     * @brief returns the optional description of this element.
     * @return the description
     * @return empty string if the description is absent
     */
    [[nodiscard]] description_type const& description() const noexcept;

    /**
     * @brief sets the optional description of this element.
     * @param description the description string, or empty to clear it
     * @return this
     */
    declaration& description(description_type description) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, declaration const& value);

private:
    std::optional<definition_id_type> definition_id_ {};
    name_type name_;
    type_pointer type_;
    value_pointer value_;
    class criteria criteria_; 
    description_type description_;
};

} // namespace yugawara::variable
