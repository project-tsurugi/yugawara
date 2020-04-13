#pragma once

#include <iostream>
#include <type_traits>
#include <utility>
#include <variant>

namespace yugawara::util {

/**
 * @brief provides either normal value or erroneous information.
 * @tparam Error the erroneous information type
 * @tparam Normal the normal value type
 */
template<class Error, class Normal>
class either {

    static_assert(!std::is_same_v<Normal, Error>);
    static_assert(!std::is_reference_v<Normal>);
    static_assert(!std::is_reference_v<Error>);

public:
    /// @brief normal value type.
    using value_type = Normal;
    /// @brief the element type of normal value.
    using element_type = value_type;
    /// @brief the pointer type of normal value.
    using pointer = std::add_pointer_t<value_type>;
    /// @brief the const pointer type of normal value.
    using const_pointer = std::add_pointer_t<std::add_const_t<value_type>>;
    /// @brief the L-value reference type of normal value.
    using reference = std::add_lvalue_reference_t<value_type>;
    /// @brief the L-value const reference type of normal value.
    using const_reference = std::add_lvalue_reference_t<std::add_const_t<value_type>>;
    /// @brief the R-value reference type of normal value.
    using rvalue_reference = std::add_rvalue_reference_t<value_type>;

    /// @brief erroneous information type.
    using error_type = Error;
    /// @brief the L-value reference type of erroneous information.
    using error_reference = std::add_lvalue_reference_t<error_type>;
    /// @brief the L-value reference type of erroneous information.
    using error_const_reference = std::add_lvalue_reference_t<std::add_const_t<error_type>>;
    /// @brief the R-value reference type of erroneous information.
    using error_rvalue_reference = std::add_rvalue_reference_t<error_type>;

    /**
     * @brief constructs a new object.
     * @details this is available only if the erroneous value type is default constructible.
     */
    either() = default;

    /**
     * @brief constructs a new object.
     * @param value the value
     */
    either(const_reference value) noexcept(std::is_nothrow_copy_constructible_v<value_type>) // NOLINT
        : alternatives_(std::in_place_index<value_index>, value)
    {}

    /**
     * @brief assigns the given value.
     * @param value the value
     * @return this
     */
    either& operator=(const_reference value) noexcept(std::is_nothrow_copy_constructible_v<value_type>) {
        alternatives_.template emplace<value_index>(value);
        return *this;
    }

    /**
     * @brief constructs a new object.
     * @param value the value
     */
    either(rvalue_reference value) noexcept(std::is_nothrow_move_constructible_v<value_type>) // NOLINT
        : alternatives_(std::in_place_index<value_index>, std::move(value))
    {}

    /**
     * @brief assigns the given value.
     * @param value the value
     * @return this
     */
    either& operator=(rvalue_reference value) noexcept(std::is_nothrow_move_constructible_v<value_type>) {
        alternatives_.template emplace<value_index>(std::move(value));
        return *this;
    }

    /**
     * @brief constructs an erroneous object.
     * @param error the error information
     */
    either(error_const_reference error) noexcept(std::is_nothrow_copy_constructible_v<error_type>) // NOLINT
        : alternatives_(std::in_place_index<error_index>, error)
    {}

    /**
     * @brief assigns the given erroneous information.
     * @param error the error information
     * @return this
     */
    either& operator=(error_const_reference error) noexcept(std::is_nothrow_copy_constructible_v<error_type>) {
        alternatives_.template emplace<error_index>(error);
        return *this;
    }

    /**
     * @brief constructs an erroneous object.
     * @param error the error information
     */
    either(error_rvalue_reference error) noexcept(std::is_nothrow_move_constructible_v<error_type>) // NOLINT
        : alternatives_(std::in_place_index<error_index>, std::move(error))
    {}

    /**
     * @brief assigns the given erroneous information.
     * @param error the error information
     * @return this
     */
    either& operator=(error_rvalue_reference error) noexcept(std::is_nothrow_move_constructible_v<error_type>) {
        alternatives_.template emplace<error_index>(std::move(error));
        return *this;
    }

    /**
     * @brief constructs a new object.
     * @tparam T the target type, must be either erroneous information or normal value type
     * @tparam Args the parameter types of target type constructor
     * @param args the constructor arguments
     */
    template<class T, class... Args>
    explicit either(std::in_place_type_t<T>, Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
        : alternatives_(std::in_place_type<T>, std::forward<Args>(args)...)
    {}

    /**
     * @brief emplaces a new normal value or erroneous information.
     * @tparam T the target type, must be either erroneous information or normal value type
     * @tparam Args the parameter types of target type constructor
     * @param args the constructor arguments
     */
    template<class T, class... Args>
    T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>) {
        return alternatives_.template emplace<T>(std::forward<Args>(args)...);
    }

    /**
     * @brief returns whether or not this object holds a normal value.
     * @return true if this object holds a normal value
     * @return false otherwise
     */
    [[nodiscard]] constexpr bool has_value() const noexcept { return alternatives_.index() == value_index; }

    /// @copydoc has_value()
    [[nodiscard]] explicit constexpr operator bool() const noexcept { return has_value(); }

    /**
     * @brief returns the holding normal value.
     * @return the normal value
     * @see has_value()
     * @warning undefined behavior if this object does not hold a normal value
     */
    [[nodiscard]] reference value() { return std::get<value_index>(alternatives_); }

    /// @copydoc value()
    [[nodiscard]] const_reference value() const { return std::get<value_index>(alternatives_); }

    /**
     * @brief returns a pointer to the holding normal value only if it exists.
     * @return pointer to the normal value
     * @return nullptr if this does not have normal value
     */
    [[nodiscard]] pointer get() noexcept { return std::get_if<value_index>(&alternatives_); }

    /// @copydoc get()
    [[nodiscard]] const_pointer get() const noexcept { return std::get_if<value_index>(&alternatives_); }

    /// @copydoc value()
    [[nodiscard]] reference operator*() { return value(); }

    /// @copydoc value()
    [[nodiscard]] const_reference operator*() const { return value(); }

    /// @copydoc get()
    [[nodiscard]] pointer operator->() noexcept { return get(); }

    /// @copydoc get()
    [[nodiscard]] const_pointer operator->() const noexcept { return get(); }

    /**
     * @brief returns whether or not this object holds an erroneous information.
     * @return true if this object holds an erroneous information
     * @return false otherwise
     */
    [[nodiscard]] constexpr bool has_error() const noexcept { return alternatives_.index() == error_index; }

    /// @copydoc has_error()
    [[nodiscard]] constexpr bool is_error() const noexcept { return has_error(); }

    /**
     * @brief returns the holding erroneous information.
     * @return the normal value
     * @see has_error()
     * @warning undefined behavior if this object does not hold erroneous information
     */
    [[nodiscard]] error_reference error() { return std::get<error_index>(alternatives_); }

    /// @copydoc error()
    [[nodiscard]] error_const_reference error() const { return std::get<error_index>(alternatives_); }

private:
    std::variant<error_type, value_type> alternatives_ { std::in_place_index<error_index> };

    static constexpr inline std::size_t value_index = 1;
    static constexpr inline std::size_t error_index = 0;
};

/**
 * @brief returns whether or not the each object has equivalent value or error information.
 * @tparam E1 the erroneous information type of the first object
 * @tparam N1 the normal value type of the first object
 * @tparam E2 the erroneous information type of the second object
 * @tparam N2 the normal value type of the second object
 * @param a the first object
 * @param b the second object
 * @return true if the both have equivalent value or error information
 * @return false otherwise
 */
template<class E1, class N1, class E2, class N2>
inline bool operator==(either<E1, N1> const& a, either<E2, N2> const& b) noexcept {
    if (a.has_value() && b.has_value()) {
        return a.value() == b.value();
    }
    if (a.has_error() && b.has_error()) {
        return a.error() == b.error();
    }
    return false;
}

/**
 * @brief returns whether or not the each object has different value or error information.
 * @tparam E1 the erroneous information type of the first object
 * @tparam N1 the normal value type of the first object
 * @tparam E2 the erroneous information type of the second object
 * @tparam N2 the normal value type of the second object
 * @param a the first object
 * @param b the second object
 * @return true if the both have different value or error information
 * @return false otherwise
 */
template<class E1, class N1, class E2, class N2>
inline bool operator!=(either<E1, N1> const& a, either<E2, N2> const& b) noexcept {
    return !(a == b);
}

/**
 * @brief appends a string representation of the given value.
 * @tparam E the erroneous information type
 * @tparam N the normal value type
 * @param out the target output stream
 * @param value the target value
 * @return the output stream
 */
template<class E, class N>
inline std::ostream& operator<<(std::ostream& out, either<E, N> const& value) {
    if (value.has_value()) {
        return out << value.value();
    }
    if (value.has_error()) {
        return out << value.error();
    }
    return out << "(broken)";
}

} // namespace yugawara::util
