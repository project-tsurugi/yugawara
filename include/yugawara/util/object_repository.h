#pragma once

#include <utility>

#include <takatori/util/clonable.h>

#include "object_cache.h"

namespace yugawara::util {

/**
 * @brief a repository of objects.
 * @tparam T the object type, must be clonable
 * @tparam Hash the hash function object type
 * @tparam KeyEqual the key comparator function object type
 */
template<class T, class Hash = std::hash<T>, class KeyEqual = std::equal_to<>>
class object_repository {

    static_assert(takatori::util::is_clonable_v<T>);

public:
    /// @brief the object type.
    using value_type = T;

    /// @brief the cache storage type.
    using cache_type = object_cache<value_type, Hash, KeyEqual>;

    /// @brief the const L-value reference type.
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

    /// @brief the R-value reference type.
    using rvalue_reference = std::add_rvalue_reference_t<value_type>;

    /**
     * @brief creates a new instance which does not have cache mechanism.
     */
    object_repository() = default;

    /**
     * @brief creates a new instance.
     * @param enable_cache whether or not cache mechanism is enabled
     */
    explicit object_repository(bool enable_cache) :
        cache_ {
            enable_cache
                ? std::make_unique<cache_type>()
                : std::unique_ptr<cache_type> {}
        }
    {}

    /**
     * @brief creates a new instance with using the given cache table.
     * @param cache the cache table
     */
    explicit object_repository(std::unique_ptr<cache_type> cache) noexcept
        : cache_(std::move(cache))
    {}

    /**
     * @brief creates a new instance with using the given cache table.
     * @param cache the cache table
     */
    explicit object_repository(cache_type cache)
        : cache_(std::make_unique<cache_type>(std::move(cache)))
    {}

    /**
     * @brief returns an object from this repository.
     * @details the returned shared pointer contains an object which equivalent (by @c KeyEqual type parameter) to
     *      the input one.
     * @tparam U the input value type
     * @param value the original object
     * @return the equivalent value in this repository
     */
    template<class U>
    [[nodiscard]] std::shared_ptr<std::remove_const_t<std::remove_reference_t<U>>> get(U&& value) {
        auto&& result = internal_get(std::forward<U>(value));
        return std::static_pointer_cast<std::remove_const_t<std::remove_reference_t<U>>>(std::move(result));
    }

    /**
     * @brief removes all entries in this repository.
     */
    void clear() {
        if (cache_) {
            cache_->clear();
        }
    }

private:
    std::unique_ptr<cache_type> cache_;

    template<class U>
    [[nodiscard]] std::shared_ptr<value_type> internal_get(U&& value) {
        if (cache_) {
            return cache_->add(std::forward<U>(value)).lock();
        }
        return takatori::util::clone_shared(std::forward<U>(value));
    }
};

/**
 * @brief template deduction guide for object_repository.
 * @tparam T the object type, must be clonable
 * @tparam Hash the hash function object type
 * @tparam KeyEqual the key comparator function object type
 */
template<class T, class Hash, class KeyEqual>
object_repository(std::unique_ptr<object_cache<T, Hash, KeyEqual>> cache)
-> object_repository<T, Hash, KeyEqual>;

/**
 * @brief template deduction guide for object_repository.
 * @tparam T the object type, must be clonable
 * @tparam Hash the hash function object type
 * @tparam KeyEqual the key comparator function object type
 */
template<class T, class Hash, class KeyEqual>
object_repository(object_cache<T, Hash, KeyEqual> cache)
-> object_repository<T, Hash, KeyEqual>;

} // namespace yugawara::util
