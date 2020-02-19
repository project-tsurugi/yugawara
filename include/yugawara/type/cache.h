#pragma once

#include <takatori/type/data.h>

#include <yugawara/util/object_cache.h>

namespace yugawara::type {

/**
 * @brief a type cache.
 */
using cache = util::object_cache<takatori::type::data>;

} // namespace yugawara::type
