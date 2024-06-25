#pragma once

#include <memory>

#include <takatori/descriptor/schema.h>
#include <takatori/util/object.h>
#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/schema/declaration.h>

namespace yugawara::binding {

/**
 * @brief refers existing schema declaration.
 */
class schema_info : public ::takatori::descriptor::schema::entity_type {
public:
    /// @brief the corresponded descriptor type.
    using descriptor_type = ::takatori::descriptor::schema;

    /// @brief the pointer type of schema declaration.
    using declaration_pointer = ::takatori::util::maybe_shared_ptr<schema::declaration const>;

    /**
     * @brief creates a new instance.
     * @param declaration the schema declaration
     */
    explicit schema_info(declaration_pointer declaration) noexcept;

    ~schema_info() override = default;
    schema_info(schema_info const& other) = delete;
    schema_info& operator=(schema_info const& other) = delete;
    schema_info(schema_info&& other) noexcept = delete;
    schema_info& operator=(schema_info&& other) noexcept = delete;

    /**
     * @brief returns the class ID.
     * @return the class ID
     */
    [[nodiscard]] std::size_t class_id() const noexcept final;

    /**
     * @brief returns the target schema declaration.
     * @return the schema declaration
     */
    [[nodiscard]] schema::declaration const& declaration() const noexcept;

    /**
     * @brief sets the target schema declaration.
     * @param declaration the target declaration
     * @return this
     */
    schema_info& declaration(declaration_pointer declaration) noexcept;

    /**
     * @brief returns the target schema declaration as shared pointer.
     * @return the schema declaration as shared pointer
     */
    [[nodiscard]] declaration_pointer const& shared_declaration() const noexcept;

    /**
     * @brief returns whether or not the two schemas are equivalent.
     * @param a the first schema
     * @param b the second schema
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(schema_info const& a, schema_info const& b) noexcept;

    /**
     * @brief returns whether or not the two schemas are different.
     * @param a the first schema
     * @param b the second schema
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(schema_info const& a, schema_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, schema_info const& value);

protected:
    /**
     * @brief returns whether or not this object is equivalent to the target one.
     * @param other the target object
     * @return true if the both are equivalent
     * @return false otherwise
     */
    [[nodiscard]] bool equals(descriptor_type::entity_type const& other) const noexcept override;

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
[[nodiscard]] schema_info::descriptor_type wrap(std::shared_ptr<schema_info> info) noexcept;

/**
 * @brief extracts information from the descriptor.
 * @param descriptor the target descriptor
 * @return the corresponded object information
 * @warning undefined behavior if the descriptor was broken
 */
[[nodiscard]] schema_info& unwrap(schema_info::descriptor_type const& descriptor);

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::schema_info.
 */
template<>
struct std::hash<yugawara::binding::schema_info> : public std::hash<::yugawara::binding::schema_info::descriptor_type::entity_type> {};
