#pragma once

#include <takatori/descriptor/relation.h>

#include <takatori/util/object.h>
#include <takatori/util/object_creator.h>

#include "relation_info_kind.h"

namespace yugawara::binding {

/**
 * @brief an abstract interface of binding for external relations.
 */
class relation_info : public ::takatori::util::object {
public:
    /// @brief the corresponded descriptor type.
    using descriptor_type = ::takatori::descriptor::relation;

    /**
     * @brief creates a new instance.
     */
    relation_info() = default;
    ~relation_info() override = default;
    relation_info(relation_info const& other) = delete;
    relation_info& operator=(relation_info const& other) = delete;
    relation_info(relation_info&& other) noexcept = delete;
    relation_info& operator=(relation_info&& other) noexcept = delete;

    /**
     * @brief returns the kind of this relation.
     * @return the relation kind
     */
    virtual relation_info_kind kind() const noexcept = 0;

    /**
     * @brief returns a clone of this object.
     * @param creator the object creator
     * @return the created clone
     */
    virtual relation_info* clone(takatori::util::object_creator creator) const& = 0;

    /// @copydoc clone()
    virtual relation_info* clone(takatori::util::object_creator creator) && = 0;

    /**
     * @brief returns whether or not the two relations are equivalent.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(relation_info const& a, relation_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(relation_info const& a, relation_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, relation_info const& value);

protected:
    /**
     * @brief returns whether or not this object is equivalent to the target one.
     * @param other the target object
     * @return true if the both are equivalent
     * @return false otherwise
     */
    bool equals(object const& other) const noexcept override;

    /**
     * @brief returns whether or not this object is equivalent to the target one.
     * @param other the target object
     * @return true if the both are equivalent
     * @return false otherwise
     */
    virtual bool equals(relation_info const& other) const noexcept = 0;

    /**
     * @brief returns a hash code of this object.
     * @return the computed hash code
     */
    std::size_t hash() const noexcept override = 0;

    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    std::ostream& print_to(std::ostream& out) const override = 0;
};

/**
 * @brief extracts information from the descriptor.
 * @param descriptor the target descriptor
 * @return the corresponded object information
 * @warning undefined behavior if the descriptor was broken
 */
relation_info& extract(relation_info::descriptor_type const& descriptor);

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::relation_info.
 */
template<>
struct std::hash<yugawara::binding::relation_info> : public std::hash<takatori::util::object> {};
