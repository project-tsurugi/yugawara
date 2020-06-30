#pragma once

#include "basic_configurable_provider.h"

namespace yugawara::type {

/**
 * @brief an implementation of user-defined type declaration provider that can configure its contents.
 * @note This class works as thread-safe.
 */
using configurable_provider = basic_configurable_provider<std::less<>, std::mutex>;

} // namespace yugawara::type
