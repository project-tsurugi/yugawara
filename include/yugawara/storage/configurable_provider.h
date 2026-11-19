#pragma once

#include <yugawara/util/sharded_shared_mutex.h>

#include "basic_configurable_provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of storage information provider that can configure its contents.
 * @note This class works as thread-safe.
 * @note This uses util::sharded_shared_mutex, because the storage information is frequently read from
 *      many threads but is rarely modified.
 */
using configurable_provider = basic_configurable_provider<util::sharded_shared_mutex>;

} // namespace yugawara::storage
