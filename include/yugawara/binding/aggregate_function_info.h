#pragma once

#include <takatori/descriptor/aggregate_function.h>
#include <takatori/util/object.h>
#include <takatori/util/object_creator.h>

#include <yugawara/aggregate/declaration.h>

namespace yugawara::binding {

/**
 * @brief refers existing aggregate function declaration.
 */
class aggregate_function_info : public ::takatori::util::object {
public:
    /// @brief the corresponded descriptor type.
    using descriptor_type = ::takatori::descriptor::aggregate_function;

    /// @brief the pointer type of function declaration.
    using declaration_pointer = std::shared_ptr<aggregate::declaration const>;

    /**
     * @brief creates a new instance.
     * @param declaration the function declaration
     */
    explicit aggregate_function_info(declaration_pointer declaration) noexcept;

    ~aggregate_function_info() override = default;
    aggregate_function_info(aggregate_function_info const& other) = delete;
    aggregate_function_info& operator=(aggregate_function_info const& other) = delete;
    aggregate_function_info(aggregate_function_info&& other) noexcept = delete;
    aggregate_function_info& operator=(aggregate_function_info&& other) noexcept = delete;

    /**
     * @brief returns the target function declaration.
     * @return the function declaration
     */
    [[nodiscard]] aggregate::declaration const& declaration() const noexcept;

    /**
     * @brief sets the target function declaration.
     * @param declaration the target declaration
     * @return this
     */
    aggregate_function_info& declaration(declaration_pointer declaration) noexcept;

    /**
     * @brief returns the target function declaration as shared pointer.
     * @return the function declaration as shared pointer
     */
    [[nodiscard]] declaration_pointer const& shared_declaration() const noexcept;

    /**
     * @brief returns whether or not the two functions are equivalent.
     * @param a the first function
     * @param b the second function
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(aggregate_function_info const& a, aggregate_function_info const& b) noexcept;

    /**
     * @brief returns whether or not the two functions are different.
     * @param a the first function
     * @param b the second function
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(aggregate_function_info const& a, aggregate_function_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, aggregate_function_info const& value);
    
protected:
    /**
     * @brief returns whether or not this object is equivalent to the target one.
     * @param other the target object
     * @return true if the both are equivalent
     * @return false otherwise
     */
    [[nodiscard]] bool equals(object const& other) const noexcept override;

    /**
     * @brief returns a hash code of this object.
     * @return the computed hash code
     */
    [[nodiscard]] std::size_t hash() const noexcept override;

    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    std::ostream& print_to(std::ostream& out) const override;

private:
    declaration_pointer declaration_;
};

/**
 * @brief wraps information as a descriptor.
 * @param info the source information
 * @return the wrapped descriptor
 */
[[nodiscard]] aggregate_function_info::descriptor_type wrap(std::shared_ptr<aggregate_function_info> info) noexcept;

/**
 * @brief extracts information from the descriptor.
 * @param descriptor the target descriptor
 * @return the corresponded object information
 * @warning undefined behavior if the descriptor was broken
 */
[[nodiscard]] aggregate_function_info& unwrap(aggregate_function_info::descriptor_type const& descriptor);

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::aggregate_function_info.
 */
template<>
struct std::hash<yugawara::binding::aggregate_function_info> : public std::hash<takatori::util::object> {};
