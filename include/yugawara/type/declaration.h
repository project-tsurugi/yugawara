#pragma once

#include <memory>
#include <ostream>
#include <optional>
#include <string>

#include <cstddef>

#include <takatori/type/data.h>
#include <takatori/util/optional_ptr.h>

namespace yugawara::type {

/**
 * @brief represents a user-defined declaration.
 * @details FIXME: impl UDT declaration
 */
class declaration {
public:
    /// @brief the function definition ID type.
    using definition_id_type = std::size_t;

    /// @brief the declaration name type.
    using name_type = std::string;

    /// @brief the smart pointer of type.
    using type_pointer = std::shared_ptr<takatori::type::data const>;

    /**
     * @brief creates a new object.
     * @param definition_id the definition ID
     * @param name the type name
     */
    explicit declaration(
            std::optional<definition_id_type> definition_id,
            name_type name) noexcept;

    /**
     * @brief returns the type definition ID.
     * @return the definition ID
     * @return empty if it is not specified
     */
    [[nodiscard]] std::optional<definition_id_type> definition_id() const noexcept;

    /**
     * @brief sets the type definition ID.
     * @param definition_id the definition ID
     * @return this
     */
    declaration& definition_id(std::optional<definition_id_type> definition_id) noexcept;

    /**
     * @brief returns the type name.
     * @return the type name
     */
    [[nodiscard]] std::string_view name() const noexcept;

    /**
     * @brief sets the type name.
     * @param name the type name
     * @return this
     */
    declaration& name(name_type name) noexcept;

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
};

/**
 * @brief returns whether or not the two types are same.
 * @param a the first schema
 * @param b the second schema
 * @return true if the both are same
 * @return false otherwise
 */
constexpr bool operator==(declaration const& a, declaration const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

/**
 * @brief returns whether or not the two types are different.
 * @param a the first schema
 * @param b the second schema
 * @return true if the both are different
 * @return false otherwise
 */
constexpr bool operator!=(declaration const& a, declaration const& b) noexcept {
    return !(a == b);
}

} // namespace yugawara::type
