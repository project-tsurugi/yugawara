#pragma once

#include <mutex>
#include <shared_mutex>

#include <takatori/util/meta_type.h>

namespace yugawara::util {

/// @private
namespace impl {

/**
 * @brief provides shared lock type only if it is std::shared_mutex.
 * @tparam Mutex the mutex type
 */
template <class Mutex>
struct maybe_shared_lock : ::takatori::util::meta_type<std::unique_lock<Mutex>> {};

/**
 * @brief always provides shared lock type.
 */
template <>
struct maybe_shared_lock<std::shared_mutex> : ::takatori::util::meta_type<std::shared_lock<std::shared_mutex>> {};

} // namespace impl

/**
 * @brief provides shared lock type only if it is std::shared_mutex.
 * @tparam Mutex the mutex type
 */
template <class Mutex>
using maybe_shared_lock = typename impl::maybe_shared_lock<Mutex>::type;

} // namespace yugawara::util
