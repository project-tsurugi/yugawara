#pragma once

#include <takatori/type/data.h>

#include "cache.h"

#include <yugawara/util/object_repository.h>

namespace yugawara::type {

/**
 * @brief a type repository.
 */
using repository = util::object_repository<takatori::type::data>;

/**
 * @brief returns the default type repository.
 * @details this repository never holds cache of types.
 * @return the default repository
 */
repository& default_repository() noexcept;

} // namespace yugawara::type
