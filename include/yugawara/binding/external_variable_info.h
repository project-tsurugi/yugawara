#pragma once

#include <memory>

#include "variable_info.h"
#include "variable_info_kind.h"

#include <yugawara/variable/declaration.h>

namespace yugawara::binding {

/**
 * @brief refers existing external variables.
 */
class external_variable_info : public variable_info {
public:
    /// @brief the pointer type of function declaration.
    using declaration_pointer = std::shared_ptr<variable::declaration const>;

    /// @brief the variable kind.
    static constexpr variable_info_kind tag = variable_info_kind::external_variable;

    /**
     * @brief creates a new object.
     * @param declaration the original declaration
     */
    explicit external_variable_info(declaration_pointer declaration) noexcept;

    /**
     * @brief returns the class ID.
     * @return the class ID
     */
    [[nodiscard]] std::size_t class_id() const noexcept final;

    [[nodiscard]] variable_info_kind kind() const noexcept override;

    /**
     * @brief returns the target variable declaration.
     * @return the variable declaration
     */
    [[nodiscard]] variable::declaration const& declaration() const noexcept;

    /**
     * @brief sets the target variable declaration.
     * @param declaration the target declaration
     * @return this
     */
    external_variable_info& declaration(declaration_pointer declaration) noexcept;

    /**
     * @brief returns the target variable declaration as shared pointer.
     * @return the variable declaration as shared pointer
     */
    [[nodiscard]] declaration_pointer const& shared_declaration() const noexcept;

    /**
     * @brief returns whether or not the two relations are equivalent.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(external_variable_info const& a, external_variable_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(external_variable_info const& a, external_variable_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, external_variable_info const& value);

protected:
    [[nodiscard]] bool equals(::takatori::descriptor::variable::entity_type const& other) const noexcept override;
    [[nodiscard]] std::size_t hash() const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    declaration_pointer declaration_;
};

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::external_variable_info.
 */
template<>
struct std::hash<yugawara::binding::external_variable_info> : public std::hash<yugawara::binding::variable_info> {};
