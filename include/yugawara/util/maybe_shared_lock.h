#pragma once

#include <mutex>
#include <shared_mutex>
#include <type_traits>
#include <utility>

#include <takatori/util/meta_type.h>

namespace yugawara::util {

/// @private
namespace impl {

/**
 * @brief provides shared lock type only if the mutex supports shared locks.
 * @tparam Mutex the mutex type
 */
template <class Mutex, class = void>
struct maybe_shared_lock : ::takatori::util::meta_type<std::unique_lock<Mutex>> {};

/**
 * @brief always provides shared lock type, for mutex types which have lock_shared() and unlock_shared().
 */
template <class Mutex>
struct maybe_shared_lock<Mutex, std::void_t<
        decltype(std::declval<Mutex&>().lock_shared()),
        decltype(std::declval<Mutex&>().unlock_shared())>>
    : ::takatori::util::meta_type<std::shared_lock<Mutex>> {};

} // namespace impl

/**
 * @brief provides shared lock type only if the mutex supports shared locks (e.g. std::shared_mutex).
 * @tparam Mutex the mutex type
 */
template <class Mutex>
using maybe_shared_lock = typename impl::maybe_shared_lock<Mutex>::type;

} // namespace yugawara::util
