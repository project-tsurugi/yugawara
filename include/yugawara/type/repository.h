#pragma once

#include <memory>
#include <type_traits>

#include <takatori/type/data.h>

#include "yugawara/util/object_repository.h"

namespace yugawara::type {

/**
 * @brief a type repository.
 */
using repository = util::object_repository<takatori::type::data>;

} // namespace yugawara::type
