#pragma once

#include <takatori/type/data.h>

#include <takatori/scalar/comparison_operator.h>

namespace yugawara::type {

/**
 * @brief returns whether the type is comparable for the comparison operator.
 * @param comparison the comparison operator
 * @param type the target type
 * @return true if the type is comparable for the operator
 * @return false otherwise
 */
bool is_comparable(::takatori::scalar::comparison_operator comparison, ::takatori::type::data const& type) noexcept;

/**
 * @brief returns whether the type is equality comparable.
 * @param type the target type
 * @return true if the type is equality comparable
 * @return false otherwise
 */
bool is_equality_comparable(::takatori::type::data const& type) noexcept;

/**
 * @brief returns whether the type is order comparable.
 * @param type the target type
 * @return true if the type is order comparable
 * @return false otherwise
 */
bool is_order_comparable(::takatori::type::data const& type) noexcept;

} // namespace yugawara::type
