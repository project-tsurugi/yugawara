#pragma once

#include <stdexcept>
#include <type_traits>

#include <takatori/relation/expression.h>

#include "block_expression_util.h"

namespace yugawara::analyzer::details {

template<class T>
class block_expression_iterator;

template<class T1, class T2>
inline constexpr bool operator==(block_expression_iterator<T1> a, block_expression_iterator<T2> b) noexcept;

template<class T1, class T2>
inline constexpr bool operator!=(block_expression_iterator<T1> a, block_expression_iterator<T2> b) noexcept;

/**
 * @brief iterates over a series of blocks.
 * @tparam T the block type
 */
template<class T>
class block_expression_iterator {
public:
    /// @brief the iterator type
    using iterator_type = block_expression_iterator;

    /// @brief the value type
    using value_type = T;
    /// @brief the size type
    using size_type = std::size_t;
    /// @brief the difference type
    using difference_type = std::ptrdiff_t;

    /// @brief the L-value reference type
    using reference = std::add_lvalue_reference_t<value_type>;
    /// @brief the const L-value reference type
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;

    /// @brief the pointer type
    using pointer = std::add_pointer_t<value_type>;
    /// @brief the const pointer type
    using const_pointer = std::add_pointer_t<std::add_const_t<value_type>>;

    /// @brief the iterator category tag.
    using iterator_category = std::bidirectional_iterator_tag;

    /**
     * @brief creates a new object.
     * @param cursor pointer of the target element (may by null)
     */
    explicit constexpr block_expression_iterator(pointer cursor) noexcept
        : cursor_(cursor)
    {}

    /**
     * @brief creates a new object.
     * @tparam U the element type of the source iterator
     * @param other the source iterator
     */
    template<
            class U,
            class = std::enable_if_t<
                    std::is_constructible_v<
                            pointer,
                            typename block_expression_iterator<U>::pointer>>>
    constexpr block_expression_iterator(block_expression_iterator<U> other) noexcept // NOLINT
        : block_expression_iterator(other.cursor_)
    {}

    /**
     * @brief returns the value reference where this iterator pointing.
     * @return reference of the current value
     */
    [[nodiscard]] constexpr reference operator*() const noexcept {
        return *cursor_;
    }

    /**
     * @brief returns pointer to the value where this iterator pointing.
     * @return pointer to the current value
     */
    [[nodiscard]] constexpr pointer operator->() const noexcept {
        return cursor_;
    }

    /**
     * @brief increments this iterator position.
     * @return this
     */
    iterator_type& operator++() noexcept {
        if (cursor_ == nullptr) {
            return *this;
        }
        cursor_ = downstream_unambiguous(*cursor_).get();
        return *this;
    }

    /**
     * @brief increments this iterator position.
     * @return the last position
     */
    iterator_type const operator++(int) noexcept { // NOLINT
        iterator_type r { *this };
        operator++();
        return r;
    }

    /**
     * @brief decrements this iterator position.
     * @return this
     */
    iterator_type& operator--() noexcept {
        if (cursor_ == nullptr) {
            return *this;
        }
        cursor_ = upstream_unambiguous(*cursor_).get();
        return *this;
    }

    /**
     * @brief decrements this iterator position.
     * @return the last position
     */
    iterator_type const operator--(int) noexcept { // NOLINT
        iterator_type r { *this };
        operator--();
        return r;
    }

private:
    pointer cursor_;

    template<class U> friend class block_expression_iterator;

    template<class T1, class T2>
    friend constexpr bool operator==(block_expression_iterator<T1> a, block_expression_iterator<T2> b) noexcept; // NOLINT(readability-redundant-declaration)

    template<class T1, class T2>
    friend constexpr bool operator!=(block_expression_iterator<T1> a, block_expression_iterator<T2> b) noexcept; // NOLINT(readability-redundant-declaration)
};

/**
 * @brief returns whether or not the two iterator points the same position.
 * @tparam T1 value type of the first iterator
 * @tparam T2 value type of the second iterator
 * @param a the first iterator
 * @param b the second iterator
 * @return true if the two iterator points the same position
 * @return false otherwise
 */
template<class T1, class T2>
inline constexpr bool
operator==(block_expression_iterator<T1> a, block_expression_iterator<T2> b) noexcept {
    return a.cursor_ == b.cursor_;
}

/**
 * @brief returns whether or not the two iterator points the different position.
 * @tparam T1 value type of the first iterator
 * @tparam T2 value type of the second iterator
 * @param a the first iterator
 * @param b the second iterator
 * @return true if the two iterator points the different position
 * @return false otherwise
 */
template<class T1, class T2>
inline constexpr bool
operator!=(block_expression_iterator<T1> a, block_expression_iterator<T2> b) noexcept {
    return a.cursor_ != b.cursor_;
}

} // namespace yugawara::analyzer::details
