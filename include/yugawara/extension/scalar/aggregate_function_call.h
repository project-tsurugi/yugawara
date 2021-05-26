#pragma once

#include <initializer_list>
#include <ostream>
#include <string>
#include <string_view>

#include <takatori/scalar/extension.h>
#include <takatori/tree/tree_element_vector.h>
#include <takatori/descriptor/aggregate_function.h>

#include <takatori/util/clone_tag.h>
#include <takatori/util/meta_type.h>
#include <takatori/util/reference_vector.h>
#include <takatori/util/rvalue_reference_wrapper.h>

#include "extension_id.h"

namespace yugawara::extension::scalar {

/**
 * @brief pseudo aggregate function call expression.
 */
class aggregate_function_call final : public ::takatori::scalar::extension {
public:
    /// @brief the extension ID of this type.
    static constexpr extension_id_type extension_tag = aggregate_function_call_id;

    /**
     * @brief creates a new object.
     * @param function the descriptor of the target function
     * @param arguments the argument expressions
     */
    explicit aggregate_function_call(
            ::takatori::descriptor::aggregate_function function,
            ::takatori::util::reference_vector<expression> arguments) noexcept;

    /**
     * @brief creates a new object.
     * @param function the descriptor of the target function
     * @param arguments the argument expressions
     * @attention this may take copies of given expressions
     */
    explicit aggregate_function_call(
            ::takatori::descriptor::aggregate_function function,
            std::initializer_list<::takatori::util::rvalue_reference_wrapper<expression>> arguments = {});

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit aggregate_function_call(::takatori::util::clone_tag_t, aggregate_function_call const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit aggregate_function_call(::takatori::util::clone_tag_t, aggregate_function_call&& other);

    /**
     * @brief returns a clone of this object.
     * @return the created clone
     */
    [[nodiscard]] aggregate_function_call* clone() const& override;

    /// @copydoc clone()
    [[nodiscard]] aggregate_function_call* clone() && override;

    /**
     * @brief returns the extension ID of this type.
     * @return the extension ID
     */
    [[nodiscard]] extension_id_type extension_id() const noexcept override;

    /**
     * @brief returns the descriptor of target function.
     * @return the destination type
     */
    [[nodiscard]] ::takatori::descriptor::aggregate_function& function() noexcept;

    /// @copydoc function()
    [[nodiscard]] ::takatori::descriptor::aggregate_function const& function() const noexcept;

    /**
     * @brief returns the function arguments.
     * @return the function arguments
     */
    [[nodiscard]] ::takatori::tree::tree_element_vector<expression>& arguments() noexcept;

    /// @copydoc arguments()
    [[nodiscard]] ::takatori::tree::tree_element_vector<expression> const& arguments() const noexcept;

protected:
    /**
     * @brief returns whether or not this extension is equivalent to the target one.
     * @param other the target extension
     * @return true if the both are equivalent
     * @return false otherwise
     */
    [[nodiscard]] bool equals(extension const& other) const noexcept override;

    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    std::ostream& print_to(std::ostream& out) const override;

private:
    ::takatori::descriptor::aggregate_function function_;
    ::takatori::tree::tree_element_vector<expression> arguments_;
};

/**
 * @brief returns whether or not the two elements are equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if a == b
 * @return false otherwise
 */
bool operator==(aggregate_function_call const& a, aggregate_function_call const& b) noexcept;

/**
 * @brief returns whether or not the two elements are different.
 * @param a the first element
 * @param b the second element
 * @return true if a != b
 * @return false otherwise
 */
bool operator!=(aggregate_function_call const& a, aggregate_function_call const& b) noexcept;

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
std::ostream& operator<<(std::ostream& out, aggregate_function_call const& value);

} // namespace yugawara::extension::scalar
