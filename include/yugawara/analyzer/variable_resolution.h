#pragma once

#include <functional>
#include <variant>
#include <memory>

#include "variable_resolution_kind.h"

#include <takatori/scalar/expression.h>
#include <takatori/util/meta_type.h>

#include <yugawara/storage/column.h>
#include <yugawara/variable/declaration.h>
#include <yugawara/function/declaration.h>
#include <yugawara/aggregate/declaration.h>
#include <takatori/type/data.h>
#include <yugawara/type/repository.h>

namespace yugawara::analyzer {

/**
 * @brief wraps the type by reference_wrapper only if the target type is a raw reference.
 * @tparam T the target type
 */
template<class T>
struct wrap_if_reference
    : std::conditional<
            std::is_lvalue_reference_v<T>,
            std::reference_wrapper<std::remove_reference_t<T>>,
            T>
{};

/// @copydoc wrap_if_reference
template<class T>
using wrap_if_reference_t = typename wrap_if_reference<T>::type;

/**
 * @brief provides element type of the std::reference_wrapper.
 * @details This is declaration for other than the wrapped type, so that this just provides the original type.
 * @tparam T the target type
 */
template<class T>
struct unwrap_if_reference_wrapper : ::takatori::util::meta_type<T> {};

/**
 * @brief provides element type of the std::reference_wrapper.
 * @tparam T the target type
 */
template<class T>
struct unwrap_if_reference_wrapper<std::reference_wrapper<T>> : ::takatori::util::meta_type<T> {};

/**
 * @brief provides element type of the std::reference_wrapper.
 * @tparam T the target type
 */
template<class T>
using unwrap_if_reference_wrapper_t = typename unwrap_if_reference_wrapper<T>::type;

/**
 * @brief represents information of resolved variables.
 */
class variable_resolution {
public:
    /// @brief the element kind.
    using kind_type = variable_resolution_kind;

    /// @brief the element type of variable_resolution_kind::unknown.
    using unknown_type = std::shared_ptr<::takatori::type::data const>;

    /// @brief the element type of variable_resolution_kind::scalar_expression.
    using scalar_expression_type = ::takatori::scalar::expression const&;

    /// @brief the element type of variable_resolution_kind::table_column.
    using table_column_type = storage::column const&;

    /// @brief the element type of variable_resolution_kind::external.
    using external_type = variable::declaration const&;

    /// @brief the element type of variable_resolution_kind::function_call.
    using function_call_type = function::declaration const&;

    /// @brief the element type of variable_resolution_kind::aggregation.
    using aggregation_type = aggregate::declaration const&;

    /// @brief the entity type
    using entity_type = std::variant<
            std::monostate, // unresolved
            wrap_if_reference_t<unknown_type>,
            wrap_if_reference_t<scalar_expression_type>,
            wrap_if_reference_t<table_column_type>,
            wrap_if_reference_t<external_type>,
            wrap_if_reference_t<function_call_type>,
            wrap_if_reference_t<aggregation_type>>;

    /**
     * @brief the element type for each kind.
     * @tparam Kind the element kind
     */
    template<variable_resolution_kind Kind>
    using element_type = unwrap_if_reference_wrapper_t<
            std::variant_alternative_t<
                    static_cast<std::size_t>(Kind),
                    entity_type>>;

    /**
     * @brief creates a new unresolved instance.
     */
    constexpr variable_resolution() noexcept = default;

    /**
     * @brief creates a new instance.
     * @details this resolution information will be marked as `unknown`.
     * @param type the resolved type
     */
    variable_resolution(std::shared_ptr<::takatori::type::data const> type) noexcept; // NOLINT

    /**
     * @brief creates a new instance.
     * @details this resolution information will be marked as `unknown`.
     * @param type the resolved type
     * @param repo the type repository
     */
    variable_resolution(::takatori::type::data&& type, type::repository& repo = type::default_repository()) noexcept; // NOLINT

    /**
     * @brief creates a new instance.
     * @param element the resolution source element
     */
    constexpr variable_resolution(scalar_expression_type element) noexcept // NOLINT
        : entity_(std::in_place_index<static_cast<std::size_t>(kind_type::scalar_expression)>, element)
    {}


    /**
     * @brief creates a new instance.
     * @param element the resolution source element
     */
    constexpr variable_resolution(table_column_type element) noexcept // NOLINT
        : entity_(std::in_place_index<static_cast<std::size_t>(kind_type::table_column)>, element)
    {}

    /**
     * @brief creates a new instance.
     * @param element the resolution source element
     */
    constexpr variable_resolution(external_type element) noexcept // NOLINT
        : entity_(std::in_place_index<static_cast<std::size_t>(kind_type::external)>, element)
    {}

    /**
     * @brief creates a new instance.
     * @param element the resolution source element
     */
    constexpr variable_resolution(function_call_type element) noexcept // NOLINT
        : entity_(std::in_place_index<static_cast<std::size_t>(kind_type::function_call)>, element)
    {}

    /**
     * @brief creates a new instance.
     * @param element the resolution source element
     */
    constexpr variable_resolution(aggregation_type element) noexcept // NOLINT
        : entity_(std::in_place_index<static_cast<std::size_t>(kind_type::aggregation)>, element)
    {}

    /**
     * @brief returns the element kind.
     * @return the element kind
     */
    [[nodiscard]] constexpr kind_type kind() const noexcept {
        auto index = entity_.index();
        if (index == std::variant_npos) {
            return kind_type::unresolved;
        }
        return static_cast<kind_type>(index);
    }

    /**
     * @brief returns whether or not this represents resolved information.
     * @return true if this represents resolved information
     * @return false if this is unresolved information
     */
    [[nodiscard]] constexpr bool is_resolved() const noexcept {
        return kind() != kind_type::unresolved;
    }

    /// @copydoc is_resolved()
    [[nodiscard]] explicit constexpr operator bool() const noexcept {
        return is_resolved();
    }

    /**
     * @brief returns the holding element.
     * @tparam Kind the element kind
     * @return the holding element
     * @throws std::bad_variant_access if the holding element kind is inconsistent
     */
    template<kind_type Kind>
    [[nodiscard]] std::add_lvalue_reference_t<std::add_const_t<element_type<Kind>>> element() const {
        if constexpr (Kind == kind_type::unresolved) { // NOLINT
            if (entity_.index() == std::variant_npos) {
                static std::monostate unresolved;
                return unresolved;
            }
        }
        return std::get<static_cast<std::size_t>(Kind)>(entity_);
    }

    /**
     * @brief returns the holding element.
     * @tparam Kind the element kind
     * @return the holding element
     * @return empty if the holding element kind is inconsistent
     */
    template<kind_type Kind>
    [[nodiscard]] ::takatori::util::optional_ptr<std::add_const_t<element_type<Kind>>> element_if() const {
        if constexpr (Kind == kind_type::unresolved) { // NOLINT
            return { element<Kind>() };
        }
        if (auto* p = std::get_if<static_cast<std::size_t>(Kind)>(std::addressof(entity_)); p != nullptr) {
            return { *p };
        }
        return {};
    }

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, variable_resolution const& value);

private:
    entity_type entity_;
};

/**
 * @brief returns whether or not the two resolution information is equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if the both are equivalent
 * @return false otherwise
 */
bool operator==(variable_resolution const& a, variable_resolution const& b);

/**
 * @brief returns whether or not the two resolution information is different.
 * @param a the first element
 * @param b the second element
 * @return true if the both are different
 * @return false otherwise
 */
bool operator!=(variable_resolution const& a, variable_resolution const& b);

} // namespace yugawara::analyzer
