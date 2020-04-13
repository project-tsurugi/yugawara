#pragma once

#include "relation_info.h"
#include "relation_info_kind.h"

#include <yugawara/storage/index.h>

namespace yugawara::binding {

/**
 * @brief refers existing indices.
 */
class index_info : public relation_info {
public:
    /// @brief the external relation kind.
    static constexpr relation_info_kind tag = relation_info_kind::index;

    /**
     * @brief creates a new object.
     * @param declaration the original declaration
     */
    explicit index_info(storage::index const& declaration) noexcept;

    /**
     * @brief creates a new object.
     * @param other the copy source
     * @param creator the object creator
     */
    explicit index_info(index_info const& other, ::takatori::util::object_creator creator);

    /**
     * @brief creates a new object.
     * @param other the move source
     * @param creator the object creator
     */
    explicit index_info(index_info&& other, ::takatori::util::object_creator creator);

    [[nodiscard]] kind_type kind() const noexcept override;
    [[nodiscard]] index_info* clone(takatori::util::object_creator creator) const& override;
    [[nodiscard]] index_info* clone(takatori::util::object_creator creator) && override;

    /**
     * @brief returns the origin of the index.
     * @details If the target index step was disposed, this object will become invalid.
     * @return the index
     */
    [[nodiscard]] storage::index const& declaration() const noexcept;

    /// FIXME: columns

    /**
     * @brief returns whether or not the both indices are identical.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(index_info const& a, index_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(index_info const& a, index_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, index_info const& value);

protected:
    [[nodiscard]] bool equals(relation_info const& other) const noexcept override;
    [[nodiscard]] std::size_t hash() const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    storage::index const* declaration_;
};

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::index_info.
 */
template<>
struct std::hash<yugawara::binding::index_info> : public std::hash<yugawara::binding::relation_info> {};
