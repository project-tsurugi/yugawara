#pragma once

#include "compiler_extension.h"

namespace yugawara::type::extensions {

/**
 * @brief represents an error while analyzing program.
 */
using error = compiler_extension<extension_kind::error_id>;

/**
 * @brief returns whether or not the given type model represents an erroneous type.
 * @param type the target type
 * @return true if the type is error
 * @return false otherwise
 */
inline bool is_error(takatori::type::data const& type) noexcept {
    return error::is_instance(type);
}

} // namespace yugawara::type::extensions
