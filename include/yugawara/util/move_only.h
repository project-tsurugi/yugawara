#pragma once

#include <utility>

namespace yugawara::util {

/**
 * @brief make a value move only.
 * @tparam T the value type
 */
template<class T>
class move_only {
public:
    /// @brief the value type.
    using value_type = T;

    /**
     * @brief creates a new default instance.
     */
    move_only() = default;

    /**
     * @brief creates a new instance.
     * @param value the value
     */
    move_only(T value) noexcept(std::is_nothrow_move_constructible_v<T>) : // NOLINT: implicit conversion
        value_(std::move(value))
    {}

    /**
     * @brief creates a new object.
     */
    ~move_only() = default;

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    move_only(move_only const& other) = delete;

    /**
     * @brief assigns the given object.
     * @param other the copy source
     * @return this
     */
    move_only& operator=(move_only const& other) = delete;

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    move_only(move_only&& other) noexcept(std::is_nothrow_move_constructible_v<T>) = default;

    /**
     * @brief assigns the given object.
     * @param other the move source
     * @return this
     */
    move_only& operator=(move_only&& other) noexcept(std::is_nothrow_move_constructible_v<T>) = default;


    /**
     * @brief returns the holding value.
     * @return the holding value
     */
    [[nodiscard]] value_type& value() noexcept {
        return value_;
    }

    /// @copydoc value()
    [[nodiscard]] value_type const& value() const noexcept {
        return value_;
    }

    /// @copydoc value()
    [[nodiscard]] value_type& operator*() noexcept {
        return value();
    }

    /// @copydoc value()
    [[nodiscard]] value_type const& operator*() const noexcept {
        return value();
    }

    /**
     * @brief returns the pointer to the holding value.
     * @return the holding value pointer
     */
    [[nodiscard]] value_type* operator->() noexcept {
        return std::addressof(value_);
    }

    /// @copydoc operator->()
    [[nodiscard]] value_type const* operator->() const noexcept {
        return std::addressof(value_);
    }

private:
    value_type value_ {};
};

} // namespace yugawara::util
