#pragma once

#include <functional>
#include <mutex>

#include "basic_configurable_provider.h"

namespace yugawara::function {

/**
 * @brief an implementation of function declaration provider that can configure its contents.
 * @note This class works as thread-safe.
 */
using configurable_provider = basic_configurable_provider<std::less<>, std::mutex>;

} // namespace yugawara::function
