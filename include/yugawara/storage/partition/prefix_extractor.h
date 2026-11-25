#pragma once

#include <ostream>
#include <type_traits>
#include <variant>

#include <cstddef>

#include <takatori/type/data.h>

#include <yugawara/type/repository.h>

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

/**
 * @brief returns whether or not the given kind of prefix extractor is applicable to the given source column type.
 * @details Each kind of prefix extractor has its parameter type:
 *
 *      - prefix_extractor_kind::character_prefix - `VARCHAR(*)`
 *      - prefix_extractor_kind::binary_prefix - `VARBINARY(*)`
 *      - prefix_extractor_kind::floor_division - `DECIMAL(*, *)`
 *      - prefix_extractor_kind::floor_fractional - `DECIMAL(*, *)`
 *      - prefix_extractor_kind::date_truncate - `DATE`
 *      - prefix_extractor_kind::timestamp_truncate - `TIMESTAMP` (without time zone)
 *
 *      The prefix extractor is applicable to the source column type,
 *      only if the "parameter application conversion" from the source column type to its parameter type is
 *      applicable, as same as the arguments of function calls. Such the conversion never fails.
 *
 *      prefix_extractor_kind::nothing is applicable to any types.
 * @param kind the kind of prefix extractor
 * @param source_type the source column type
 * @return true if the prefix extractor is applicable to the source column type
 * @return false otherwise, or the source column type is erroneous or pending
 * @see type::is_parameter_application_convertible()
 * @see compute_type()
 */
[[nodiscard]] bool is_applicable(prefix_extractor_kind kind, ::takatori::type::data const& source_type) noexcept;

/**
 * @brief returns the type of values computed by the given prefix extractor.
 * @details This is the type of the partition key element, that is, the type of the corresponding SQL expression
 *      of the virtual column. The literal values in partition criteria are compared as values of this type.
 *
 *      The resulting type is decided as following:
 *
 *      - prefix_extractor_kind::nothing - the source column type
 *      - prefix_extractor_kind::character_prefix - `VARCHAR(*)`
 *      - prefix_extractor_kind::binary_prefix - `VARBINARY(*)`
 *      - prefix_extractor_kind::floor_division, prefix_extractor_kind::floor_fractional
 *          - `INT` - if the source column type is `TINYINT`, `SMALLINT`, or `INT`
 *          - `BIGINT` - if the source column type is `BIGINT`
 *          - `DECIMAL(*, *)` - if the source column type is `DECIMAL`
 *      - prefix_extractor_kind::date_truncate
 *          - `INT` - if the unit is date_unit::year (the year number, as same as `EXTRACT(YEAR FROM ...)`)
 *          - `DATE` - otherwise
 *      - prefix_extractor_kind::timestamp_truncate
 *          - `INT` - if the unit is timestamp_unit::year (the year number, as same as `EXTRACT(YEAR FROM ...)`)
 *          - `DATE` - if the unit is timestamp_unit::month or timestamp_unit::day
 *          - `TIMESTAMP` (without time zone) - otherwise
 *
 *      Note that, the physical representation of the partition key element is not restricted by this type.
 * @param extractor the prefix extractor
 * @param source_type the source column type
 * @param repo the type repository
 * @return the type of values computed by the prefix extractor
 * @return erroneous type if the prefix extractor is not applicable to the source column type
 * @return pending type if the source column type is erroneous or pending
 * @see is_applicable()
 */
[[nodiscard]] std::shared_ptr<::takatori::type::data const> compute_type(
        prefix_extractor const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo = ::yugawara::type::default_repository());

} // namespace yugawara::storage::partition
