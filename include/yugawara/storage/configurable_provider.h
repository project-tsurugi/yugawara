#pragma once

#include <shared_mutex>

#include "basic_configurable_provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of storage information provider that can configure its contents.
 * @note This class works as thread-safe.
 */
using configurable_provider = basic_configurable_provider<std::shared_mutex>;

/**
 * @brief an implementation of storage information provider that can configure its contents, with a single mutex.
 * @details This uses a single std::shared_mutex for all threads.
 *      This is for the users who keep using the single mutex even if configurable_provider uses another mutex type,
 *      e.g. while migrating to it.
 * @note This class works as thread-safe.
 */
using single_mutex_configurable_provider = basic_configurable_provider<std::shared_mutex>;

} // namespace yugawara::storage
