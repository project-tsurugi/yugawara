#pragma once

#include <takatori/descriptor/variable.h>
#include <takatori/util/object.h>

#include "variable_info_kind.h"

namespace yugawara::binding {

/**
 * @brief an abstract interface of binding for variables.
 * @details Developers should not use this class directly.
 *      Please use factory and analyzer::variable_analyzer instead.
 */
class variable_info : public ::takatori::descriptor::variable::entity_type {
public:
    /// @brief the corresponded descriptor type.
    using descriptor_type = ::takatori::descriptor::variable;

    /**
     * @brief creates a new instance.
     */
    variable_info() = default;
    ~variable_info() override = default;
    variable_info(variable_info const& other) = delete;
    variable_info& operator=(variable_info const& other) = delete;
    variable_info(variable_info&& other) noexcept = delete;
    variable_info& operator=(variable_info&& other) noexcept = delete;

    /**
     * @brief returns the kind of this variable.
     * @return the variable kind
     */
    [[nodiscard]] virtual variable_info_kind kind() const noexcept = 0;

    /**
     * @brief returns whether or not the two relations are equivalent.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(variable_info const& a, variable_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(variable_info const& a, variable_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, variable_info const& value);

protected:
    /**
     * @brief returns whether or not this object is equivalent to the target one.
     * @param other the target object
     * @return true if the both are equivalent
     * @return false otherwise
     */
    [[nodiscard]] bool equals(::takatori::descriptor::variable::entity_type const& other) const noexcept override = 0;

    /**
     * @brief returns a hash code of this object.
     * @return the computed hash code
     */
    [[nodiscard]] std::size_t hash() const noexcept override = 0;

    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    std::ostream& print_to(std::ostream& out) const override = 0;
};

/**
 * @brief wraps information as a descriptor.
 * @param info the source information
 * @return the wrapped descriptor
 */
[[nodiscard]] variable_info::descriptor_type wrap(std::shared_ptr<variable_info> info) noexcept;

/**
 * @brief extracts information from the descriptor.
 * @param descriptor the target descriptor
 * @return the corresponded object information
 * @warning undefined behavior if the descriptor was broken
 */
[[nodiscard]] variable_info& unwrap(variable_info::descriptor_type const& descriptor);

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::variable_info.
 */
template<>
struct std::hash<yugawara::binding::variable_info> : public std::hash<::takatori::descriptor::variable::entity_type> {};
