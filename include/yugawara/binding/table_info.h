#pragma once

#include <memory>

#include <takatori/descriptor/storage.h>
#include <takatori/util/object.h>

#include <yugawara/storage/table.h>

namespace yugawara::binding {

/**
 * @brief refers existing table declaration.
 */
class table_info : public ::takatori::descriptor::storage::entity_type {
public:
    /// @brief the corresponded descriptor type.
    using descriptor_type = ::takatori::descriptor::storage;

    /// @brief the pointer type of table declaration.
    using declaration_pointer = std::shared_ptr<storage::table const>;

    /**
     * @brief creates a new instance.
     * @param declaration the table declaration
     */
    explicit table_info(declaration_pointer declaration) noexcept;

    ~table_info() override = default;
    table_info(table_info const& other) = delete;
    table_info& operator=(table_info const& other) = delete;
    table_info(table_info&& other) noexcept = delete;
    table_info& operator=(table_info&& other) noexcept = delete;

    /**
     * @brief returns the class ID.
     * @return the class ID
     */
    [[nodiscard]] std::size_t class_id() const noexcept final;

    /**
     * @brief returns the target table declaration.
     * @return the table declaration
     */
    [[nodiscard]] storage::table const& declaration() const noexcept;

    /**
     * @brief sets the target table declaration.
     * @param declaration the target declaration
     * @return this
     */
    table_info& declaration(declaration_pointer declaration) noexcept;

    /**
     * @brief returns the target table declaration as shared pointer.
     * @return the table declaration as shared pointer
     */
    [[nodiscard]] declaration_pointer const& shared_declaration() const noexcept;

    /**
     * @brief returns whether or not the two tables are equivalent.
     * @param a the first table
     * @param b the second table
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(table_info const& a, table_info const& b) noexcept;

    /**
     * @brief returns whether or not the two tables are different.
     * @param a the first table
     * @param b the second table
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(table_info const& a, table_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, table_info const& value);

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
[[nodiscard]] table_info::descriptor_type wrap(std::shared_ptr<table_info> info) noexcept;

/**
 * @brief extracts information from the descriptor.
 * @param descriptor the target descriptor
 * @return the corresponded object information
 * @warning undefined behavior if the descriptor was broken
 */
[[nodiscard]] table_info& unwrap(table_info::descriptor_type const& descriptor);

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::table_info.
 */
template<>
struct std::hash<yugawara::binding::table_info> : public std::hash<::yugawara::binding::table_info::descriptor_type::entity_type> {};
