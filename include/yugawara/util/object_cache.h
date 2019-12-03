#pragma once

#include <memory>
#include <unordered_map>
#include <utility>

#include <takatori/util/clonable.h>
#include <takatori/util/object_creator.h>

namespace yugawara::util {

/**
 * @brief a cache storage of objects.
 * @tparam T the object type, must be clonable
 * @tparam Hash the hash function object type
 * @tparam KeyEqual the key comparator function object type
 * @attention this class is thread @em unsafe
 */
template<class T, class Hash = std::hash<T>, class KeyEqual = std::equal_to<>>
class object_cache {

    static_assert(takatori::util::is_clonable_v<T>);

public:
    /// @brief the object type.
    using value_type = T;

    /// @brief the hash type.
    using hasher = Hash;

    /// @brief the key comparator type.
    using key_equal = KeyEqual;

    /// @brief the const L-value reference type.
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

    /// @brief the R-value reference type.
    using rvalue_reference = std::add_rvalue_reference_t<value_type>;

    /**
     * @brief creates a new instance.
     */
    object_cache() = default;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit object_cache(takatori::util::object_creator creator) noexcept
        : entries_(creator.allocator<decltype(entries_)::value_type>())
    {}

    /**
     * @brief returns the cached value of the given one.
     * @param value the value
     * @return the cached value
     * @return absent if it is not registered
     * @attention the returned std::weak_ptr will be expired after cache entries were disposed,
     *      please invoke std::weak_ptr::lock() before use it
     */
    std::weak_ptr<value_type> find(const_reference value) {
        if (auto iter = entries_.find(value); iter != entries_.end()) {
            return iter->second;
        }
        return {};
    }

    /**
     * @brief returns the cached value of the given one.
     * @details if the given value is not on the cache, this operation may create a new entry into the cache.
     * @param value the value
     * @return the cached value
     * @attention the returned std::weak_ptr will be expired after cache entries were disposed,
     *      please invoke std::weak_ptr::lock() before use it
     */
    std::weak_ptr<value_type> add(const_reference value) {
        if (auto&& cached = find(value); !cached.expired()) {
            return cached;
        }
        return internal_add_cache(value);
    }

    /// @copydoc add(const_reference)
    std::weak_ptr<value_type> add(rvalue_reference value) {
        if (auto&& cached = find(value); !cached.expired()) {
            return cached;
        }
        return internal_add_cache(std::move(value));
    }

    /**
     * @brief adds a cache entry.
     * @param entry the cache entry
     * @return the added entry, if this does not have the given object
     * @return the cached entry, if this already has the given object
     * @attention the returned std::weak_ptr will be expired after cache entries were disposed,
     *      please invoke std::weak_ptr::lock() before use it
     */
    std::weak_ptr<value_type> add(std::shared_ptr<value_type> entry) {
        if (!entry) {
            return {};
        }
        auto key = std::cref(*entry);
        auto [iter, success] = entries_.emplace(key, std::move(entry));
        (void) success;
        return iter->second;
    }

    /**
     * @brief removes all cached entry.
     * @attention this operation will invalidate all returned std::weak_ptr
     */
    void clear() {
        entries_.clear();
    }

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    takatori::util::object_creator get_object_creator() const noexcept {
        return entries_.get_allocator();
    }

private:
    std::unordered_map<
            std::reference_wrapper<value_type const>,
            std::shared_ptr<value_type>,
            hasher,
            key_equal,
            takatori::util::object_allocator<std::pair<
                    std::reference_wrapper<value_type const> const,
                    std::shared_ptr<value_type>>>> entries_ {};

    template<class U>
    std::weak_ptr<value_type> internal_add_cache(U&& value) {
        auto shared = takatori::util::clone_shared(std::forward<U>(value), get_object_creator());
        return add(std::move(shared));
    }
};

} // namespace yugawara::util
