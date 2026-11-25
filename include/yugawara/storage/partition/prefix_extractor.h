#pragma once

#include <ostream>
#include <type_traits>
#include <variant>

#include <cstddef>

#include "prefix_extractor_kind.h"
#include "character_prefix.h"
#include "binary_prefix.h"
#include "floor_division.h"
#include "floor_fractional.h"
#include "date_truncate.h"
#include "timestamp_truncate.h"

namespace yugawara::storage::partition {

/**
 * @brief represents how to compute a partition key element from its source column value.
 * @details This holds one of the prefix extraction methods (e.g. character_prefix),
 *      or nothing if the partition key element uses the whole column value.
 *      All of the prefix extraction methods are trivially copyable and never throw on copy or move,
 *      so that this object never becomes valueless.
 * @see prefix_extractor_kind
 */
class prefix_extractor {
public:
    /// @brief the element kind.
    using kind_type = prefix_extractor_kind;

    /// @brief the entity type, which must be ordered as same as prefix_extractor_kind.
    using entity_type = std::variant<
            std::monostate, // nothing
            character_prefix,
            binary_prefix,
            floor_division,
            floor_fractional,
            date_truncate,
            timestamp_truncate>;

    /**
     * @brief the element type for each kind.
     * @tparam Kind the element kind
     */
    template<kind_type Kind>
    using element_type = std::variant_alternative_t<static_cast<std::size_t>(Kind), entity_type>;

    /**
     * @brief creates a new instance which represents "no prefix extraction".
     * @see prefix_extractor_kind::nothing
     */
    constexpr prefix_extractor() noexcept = default;

    /**
     * @brief creates a new instance.
     * @param element the prefix extraction method
     * @see prefix_extractor_kind::character_prefix
     */
    constexpr prefix_extractor(character_prefix element) noexcept : entity_ { element } {} // NOLINT(*-explicit-constructor, *-explicit-conversions)

    /**
     * @brief creates a new instance.
     * @param element the prefix extraction method
     * @see prefix_extractor_kind::binary_prefix
     */
    constexpr prefix_extractor(binary_prefix element) noexcept : entity_ { element } {} // NOLINT(*-explicit-constructor, *-explicit-conversions)

    /**
     * @brief creates a new instance.
     * @param element the prefix extraction method
     * @see prefix_extractor_kind::floor_division
     */
    constexpr prefix_extractor(floor_division element) noexcept : entity_ { element } {} // NOLINT(*-explicit-constructor, *-explicit-conversions)

    /**
     * @brief creates a new instance.
     * @param element the prefix extraction method
     * @see prefix_extractor_kind::floor_fractional
     */
    constexpr prefix_extractor(floor_fractional element) noexcept : entity_ { element } {} // NOLINT(*-explicit-constructor, *-explicit-conversions)

    /**
     * @brief creates a new instance.
     * @param element the prefix extraction method
     * @see prefix_extractor_kind::date_truncate
     */
    constexpr prefix_extractor(date_truncate element) noexcept : entity_ { element } {} // NOLINT(*-explicit-constructor, *-explicit-conversions)

    /**
     * @brief creates a new instance.
     * @param element the prefix extraction method
     * @see prefix_extractor_kind::timestamp_truncate
     */
    constexpr prefix_extractor(timestamp_truncate element) noexcept : entity_ { element } {} // NOLINT(*-explicit-constructor, *-explicit-conversions)

    /**
     * @brief returns the element kind.
     * @return the element kind
     */
    [[nodiscard]] constexpr kind_type kind() const noexcept {
        return static_cast<kind_type>(entity_.index());
    }

    /**
     * @brief returns the holding element.
     * @tparam Kind the element kind
     * @return the holding element
     * @throws std::bad_variant_access if the holding element kind is inconsistent
     */
    template<kind_type Kind>
    [[nodiscard]] constexpr element_type<Kind> const& element() const {
        return std::get<static_cast<std::size_t>(Kind)>(entity_);
    }

    /**
     * @brief returns whether or not this represents a prefix extraction method.
     * @return true if this has a prefix extraction method
     * @return false if this is prefix_extractor_kind::nothing
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return kind() != kind_type::nothing;
    }

    /**
     * @brief returns whether or not the two elements are equivalent.
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(prefix_extractor const& a, prefix_extractor const& b) noexcept;

    /**
     * @brief returns whether or not the two elements are different.
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(prefix_extractor const& a, prefix_extractor const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, prefix_extractor const& value);

private:
    static_assert(static_cast<std::size_t>(kind_type::nothing) == 0);
    static_assert(std::is_same_v<element_type<kind_type::character_prefix>, character_prefix>);
    static_assert(std::is_same_v<element_type<kind_type::binary_prefix>, binary_prefix>);
    static_assert(std::is_same_v<element_type<kind_type::floor_division>, floor_division>);
    static_assert(std::is_same_v<element_type<kind_type::floor_fractional>, floor_fractional>);
    static_assert(std::is_same_v<element_type<kind_type::date_truncate>, date_truncate>);
    static_assert(std::is_same_v<element_type<kind_type::timestamp_truncate>, timestamp_truncate>);
    static_assert(std::variant_size_v<entity_type> == static_cast<std::size_t>(kind_type::timestamp_truncate) + 1);
    static_assert(std::is_trivially_copyable_v<entity_type>);
    static_assert(std::is_nothrow_copy_constructible_v<entity_type>);
    static_assert(std::is_nothrow_move_constructible_v<entity_type>);
    static_assert(std::is_nothrow_copy_assignable_v<entity_type>);
    static_assert(std::is_nothrow_move_assignable_v<entity_type>);

    entity_type entity_ {};
};

} // namespace yugawara::storage::partition
