#pragma once

#include <takatori/util/clone_tag.h>

#include "relation_info.h"
#include "relation_info_kind.h"

#include <takatori/util/maybe_shared_ptr.h>

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
     * @param declaration the original declaration
     */
    explicit index_info(::takatori::util::maybe_shared_ptr<storage::index const> declaration) noexcept;

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit index_info(::takatori::util::clone_tag_t, index_info const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit index_info(::takatori::util::clone_tag_t, index_info&& other);

    /**
     * @brief returns the class ID.
     * @return the class ID
     */
    [[nodiscard]] std::size_t class_id() const noexcept final;

    [[nodiscard]] kind_type kind() const noexcept override;
    [[nodiscard]] index_info* clone() const& override;
    [[nodiscard]] index_info* clone() && override;

    /**
     * @brief returns the origin of the index.
     * @details If the target index step was disposed, this object may become invalid.
     * @return the index
     */
    [[nodiscard]] storage::index const& declaration() const noexcept;

    /**
     * @brief returns the origin of the index.
     * @details If the target index step was disposed, this object may become invalid.
     * @return the index
     */
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<storage::index const> maybe_shared_declaration() const noexcept;

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
    [[nodiscard]] bool equals(::takatori::descriptor::relation::entity_type const& other) const noexcept override;
    [[nodiscard]] std::size_t hash() const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    ::takatori::util::maybe_shared_ptr<storage::index const> declaration_;
};

} // namespace yugawara::binding

/**
 * @brief std::hash specialization for yugawara::binding::index_info.
 */
template<>
struct std::hash<yugawara::binding::index_info> : public std::hash<yugawara::binding::relation_info> {};
