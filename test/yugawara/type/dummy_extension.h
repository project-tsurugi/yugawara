#pragma once

#include <takatori/type/extension.h>

#include <yugawara/extension/type/compiler_extension.h>
#include <yugawara/extension/type/extension_id.h>

namespace yugawara::type {

template<takatori::type::extension::extension_id_type ExtensionIdOffset>
using dummy_extension = extension::type::compiler_extension<extension::type::min_unused_id + ExtensionIdOffset>;

} // namespace yugawara::type
