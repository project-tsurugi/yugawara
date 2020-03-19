#pragma once

#include <takatori/relation/expression.h>
#include <takatori/util/optional_ptr.h>

namespace yugawara::analyzer::details {

/**
 * @brief returns the downstream unambiguous element if it exists.
 * @param element the target element
 * @return the downstream unambiguous element
 * @return nullptr if it does not exist
 */
::takatori::util::optional_ptr<::takatori::relation::expression> upstream_unambiguous(::takatori::relation::expression& element) noexcept;

/// @copydoc upstream_unambiguous()
::takatori::util::optional_ptr<::takatori::relation::expression const> upstream_unambiguous(::takatori::relation::expression const& element) noexcept;

/**
 * @brief returns the downstream unambiguous element if it exists.
 * @param element the target element
 * @return the downstream unambiguous element
 * @return nullptr if it does not exist
 */
::takatori::util::optional_ptr<::takatori::relation::expression> downstream_unambiguous(::takatori::relation::expression& element) noexcept;

/// @copydoc downstream_unambiguous()
::takatori::util::optional_ptr<::takatori::relation::expression const> downstream_unambiguous(::takatori::relation::expression const& element) noexcept;

} // namespace yugawara::analyzer::details
