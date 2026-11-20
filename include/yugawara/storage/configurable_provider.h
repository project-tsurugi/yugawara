#pragma once

#include <shared_mutex>

#include <yugawara/util/sharded_shared_mutex.h>

#include "basic_configurable_provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of storage information provider that can configure its contents.
 * @details This uses util::sharded_shared_mutex, because the storage information is frequently read
 *      from many threads but is rarely modified.
 *      The number of shards is util::sharded_shared_mutex::default_shard_count by default,
 *      and it can be specified as the second argument of the constructor, like
 *      `configurable_provider { parent, shard_count }`.
 *      Consider single_mutex_configurable_provider if the sharded mutex does not fit, e.g. while migrating to this.
 * @note This class works as thread-safe.
 * @attention The consumers of each_*() are called while holding the locks of this provider and its parents.
 *      While a thread is acquiring the exclusive lock of util::sharded_shared_mutex,
 *      the threads on the shards already acquired by the thread cannot acquire shared locks.
 *      Therefore, the consumers must not wait for other threads, including acquiring other locks.
 * @see util::sharded_shared_mutex
 */
using configurable_provider = basic_configurable_provider<util::sharded_shared_mutex>;

/**
 * @brief an implementation of storage information provider that can configure its contents, with a single mutex.
 * @details This uses a single std::shared_mutex for all threads.
 *      This is for the users who keep using the single mutex even if configurable_provider uses another mutex type,
 *      e.g. while migrating to it.
 * @note This class works as thread-safe.
 */
using single_mutex_configurable_provider = basic_configurable_provider<std::shared_mutex>;

} // namespace yugawara::storage
