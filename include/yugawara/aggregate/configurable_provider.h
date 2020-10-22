#pragma once

#include <functional>
#include <mutex>

#include "basic_configurable_provider.h"

namespace yugawara::aggregate {

/**
 * @brief an implementation of aggregate function declaration provider that can configure its contents.
 * @note This class works as thread-safe.
 */
using configurable_provider = basic_configurable_provider<std::mutex>;

} // namespace yugawara::aggregate
