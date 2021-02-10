#pragma once

#include <memory>
#include <variant>

#include <takatori/value/data.h>
#include <takatori/util/rvalue_ptr.h>

#include "sequence.h"
#include "column_value_kind.h"

namespace yugawara::storage {

/**
 * @brief represents a default value of columns.
 */
class column_value {
public:
    /// @brief the element kind.
    using kind_type = column_value_kind;

    /// @brief the entity type
    using entity_type = std::variant<
            std::monostate, // nothing
            std::shared_ptr<::takatori::value::data const>, // immediate
            std::shared_ptr<storage::sequence const> // sequence
            >;

    /**
     * @brief the element type for each kind.
     * @tparam Kind the element kind
     */
    template<kind_type Kind>
    using element_type = std::variant_alternative_t<static_cast<std::size_t>(Kind), entity_type>;

    /**
     * @brief creates a new instance which represents "no default value".
     * @see column_value_kind::nothing
     */
    constexpr column_value() noexcept = default;

    /**
     * @brief creates a new instance which represents `NULL`.
     * @see column_value_kind::null
     */
    constexpr column_value(std::nullptr_t) noexcept : // NOLINT
        entity_ { std::in_place_index<static_cast<std::size_t>(kind_type::nothing)> }
    {}

    /**
     * @brief creates a new instance which represents an immediate value.
     * @param element the element value
     * @see column_value_kind::immediate
     */
    column_value(std::shared_ptr<::takatori::value::data const> element) noexcept; // NOLINT

    /**
     * @brief creates a new instance which represents an immediate value.
     * @param element the element value
     * @see column_value_kind::immediate
     */
    column_value(::takatori::util::rvalue_ptr<::takatori::value::data> element); // NOLINT

    /**
     * @brief creates a new instance which represents a value generated by sequence.
     * @param source the source sequence
     * @see column_value_kind::sequence
     */
    column_value(std::shared_ptr<storage::sequence const> source) noexcept; // NOLINT

    /**
     * @brief returns the element kind.
     * @return the element kind
     */
    [[nodiscard]] constexpr kind_type kind() const noexcept {
        auto index = entity_.index();
        return static_cast<kind_type>(index);
    }

    /**
     * @brief returns the holding element.
     * @tparam Kind the element kind
     * @return the holding element
     * @throws std::bad_variant_access if the holding element kind is inconsistent
     */
    template<kind_type Kind>
    [[nodiscard]] element_type<Kind>const& element() const {
        return std::get<static_cast<std::size_t>(Kind)>(entity_);
    }

    /**
     * @brief returns whether or not this represents a value.
     * @return false if the element kind is column_value_kind::nothing.
     * @return true otherwise
     */
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return kind() != kind_type::nothing;
    }

private:
    entity_type entity_ {  std::in_place_index<static_cast<std::size_t>(kind_type::nothing)> };
};

/**
 * @brief returns whether or not the two column value information is equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if the both are equivalent
 * @return false otherwise
 */
bool operator==(column_value const& a, column_value const& b);

/**
 * @brief returns whether or not the two column value information is different.
 * @param a the first element
 * @param b the second element
 * @return true if the both are different
 * @return false otherwise
 */
bool operator!=(column_value const& a, column_value const& b);

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output stream
 */
std::ostream& operator<<(std::ostream& out, column_value const& value);

} // namespace yugawara::storage
