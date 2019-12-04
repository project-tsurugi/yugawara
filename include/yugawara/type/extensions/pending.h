#pragma once

#include "compiler_extension.h"

namespace yugawara::type::extensions {

/**
 * @brief represents an unresolved type that will be resolved in the future.
 * @details The pending type may appear the result of referring unresolved placeholders,
 *      or is propagated from expressions with pending type.
 */
using pending = compiler_extension<extension_kind::pending_id>;

/**
 * @brief returns whether or not the given type model represents an erroneous type.
 * @param type the target type
 * @return true if the type is pending
 * @return false otherwise
 */
inline bool is_pending(takatori::type::data const& type) noexcept {
    return pending::is_instance(type);
}

} // namespace yugawara::type::extensions
