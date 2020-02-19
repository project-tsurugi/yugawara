#pragma once

#include "basic_configurable_provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of storage information provider that can configure its contents.
 * @note This class works as thread-safe.
 */
using configurable_provider = basic_configurable_provider<std::mutex>;

} // namespace yugawara::storage
