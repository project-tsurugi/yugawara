#pragma once

#include <utility>

#include <takatori/util/clonable.h>
#include <takatori/util/object_creator.h>

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
     * @brief creates a new instance which does not have cache mechanism.
     * @param creator the object creator
     */
    explicit object_repository(takatori::util::object_creator creator) noexcept
        : cache_(nullptr, takatori::util::object_deleter { creator })
    {}

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     * @param enable_cache whether or not cache mechanism is enabled
     */
    explicit object_repository(takatori::util::object_creator creator, bool enable_cache)
        : cache_(enable_cache
                ? creator.template create_unique<cache_type>()
                : ::takatori::util::unique_object_ptr<cache_type> {})
    {}

    /**
     * @brief creates a new instance with using the given cache table.
     * @param cache the cache table
     */
    explicit object_repository(takatori::util::unique_object_ptr<cache_type> cache) noexcept
        : cache_(std::move(cache))
    {}

    /**
     * @brief creates a new instance with using the given cache table.
     * @param cache the cache table
     */
    explicit object_repository(cache_type cache)
        : cache_(cache.get_object_creator().template create_unique<cache_type>(std::move(cache)))
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

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] takatori::util::object_creator get_object_creator() const noexcept {
        return cache_.get_deleter().creator();
    }

private:
    takatori::util::unique_object_ptr<cache_type> cache_;

    template<class U>
    std::shared_ptr<value_type> internal_get(U&& value) {
        if (cache_) {
            return cache_->add(std::forward<U>(value)).lock();
        }
        return takatori::util::clone_shared(std::forward<U>(value), get_object_creator());
    }
};

/**
 * @brief template deduction guide for object_repository.
 * @tparam T the object type, must be clonable
 * @tparam Hash the hash function object type
 * @tparam KeyEqual the key comparator function object type
 */
template<class T, class Hash, class KeyEqual>
object_repository(takatori::util::unique_object_ptr<object_cache<T, Hash, KeyEqual>> cache)
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
