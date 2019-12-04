#pragma once

#include <takatori/type/extension.h>

#include "yugawara/type/extensions/compiler_extension.h"
#include "yugawara/type/extensions/extension_kind.h"

namespace yugawara::type {

template<takatori::type::extension::extension_id_type ExtensionIdOffset>
using dummy_extension = extensions::compiler_extension<extensions::min_unused_id + ExtensionIdOffset>;

} // namespace yugawara::type
