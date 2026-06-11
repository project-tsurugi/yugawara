#pragma once

#include <memory>

#include <takatori/descriptor/aggregate_function.h>

#include <takatori/scalar/immediate.h>

namespace yugawara::binding {

/**
 * @brief represents default value of aggregate functions.
 */
enum class aggregate_function_default_kind {

    /// @brief default value is not sure.
    unknown,

    /// @brief returns `NULL` if there is no input row.
    null,

    /// @brief returns `0` if there is no input row.
    zero,
};

[[nodiscard]] aggregate_function_default_kind default_kind_of(::takatori::descriptor::aggregate_function const& function);

[[nodiscard]] std::unique_ptr<::takatori::scalar::immediate> default_value_of(
        ::takatori::descriptor::aggregate_function const& function);

} // namespace yugawara::binding
