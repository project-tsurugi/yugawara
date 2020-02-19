#pragma once

#include "basic_configurable_provider.h"

namespace yugawara::variable {

/**
 * @brief an implementation of external variable declaration provider that can configure its contents.
 * @note This class works as thread-safe.
 */
using configurable_provider = basic_configurable_provider<std::less<>, std::mutex>;

} // namespace yugawara::variable
