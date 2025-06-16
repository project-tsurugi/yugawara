#pragma once

#include <memory>
#include <type_traits>
#include <vector>

namespace yugawara::analyzer::details {

/**
 * @brief provides cached container objects.
 * @tparam T the container object type, which must accept `T::allocator_type` object as its constructor parameter
 */
template<class T>
class container_pool {
public:
    /// @brief the container type.
    using value_type = T;

    /// @brief the reference type of the container.
    using reference = std::add_lvalue_reference_t<value_type>;

    /// @brief the const reference type of the container.
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

    /// @brief the container's allocator type.
    using allocator_type = typename value_type::allocator_type;

    static_assert(std::is_constructible_v<value_type, allocator_type>);

    /**
     * @brief creates a new instance.
     * @param allocator the container's allocator
     */
    explicit container_pool(allocator_type const& allocator) noexcept:
            entries_(typename decltype(entries_)::allocator_type { allocator })
    {}

    /**
     * @brief creates a new instance.
     * @param allocator the container's allocator
     */
    explicit container_pool(allocator_type&& allocator = {}) noexcept:
            entries_(typename decltype(entries_)::allocator_type { std::move(allocator) })
    {}

    /**
     * @brief acquires a container object.
     * @return the acquired object
     */
    value_type acquire() {
        if (entries_.empty()) {
            return {};
        }
        auto result = entries_.back();
        entries_.pop_back();
        return result;
    }

    /**
     * @brief releases back a container object into this pool.
     * @param entry the container object
     */
    void release(value_type&& entry) {
        entries_.emplace_back(std::move(entry));
    }

    /**
     * @brief removes all entries in this pool.
     */
    void clear() noexcept {
        entries_.clear();
    }

private:
    std::vector<
            value_type,
            typename std::allocator_traits<allocator_type>::template rebind_alloc<value_type>> entries_;
};

} // namespace yugawara::analyzer::details
