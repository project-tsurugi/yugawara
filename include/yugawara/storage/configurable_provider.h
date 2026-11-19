#pragma once

#include <yugawara/util/sharded_shared_mutex.h>

#include "basic_configurable_provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of storage information provider that can configure its contents.
 * @note This class works as thread-safe.
 * @note This uses util::sharded_shared_mutex, because the storage information is frequently read from
 *      many threads but is rarely modified.
 * @attention PoC: switching this alias from std::shared_mutex to util::sharded_shared_mutex is only for
 *      measuring the proof-of-concept code without changing its users (e.g. jogasaki).
 *      The production code will re-design how to choose the mutex type. Do not depend on this.
 */
using configurable_provider = basic_configurable_provider<util::sharded_shared_mutex>;

} // namespace yugawara::storage
