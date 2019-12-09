#pragma once

#include "basic_configurable_provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of provider that can configurable its contents.
 * @note This class works as thread-safe.
 */
using configurable_provider = basic_configurable_provider<>;

} // namespace yugawara::storage
