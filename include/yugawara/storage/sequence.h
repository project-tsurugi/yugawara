#pragma once

#include <numeric>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include <cstdint>

#include <takatori/util/optional_ptr.h>

namespace yugawara::storage {

class provider;

/**
 * @brief provides sequence information.
 */
class sequence {
public:
    /// @brief the schema definition ID type.
    using definition_id_type = std::size_t;

    /// @brief the simple name type.
    using simple_name_type = std::string;

    /// @brief the value type of sequences.
    using value_type = std::int64_t;

    /// @brief the default value of initial value.
    static constexpr value_type default_initial_value = 0;

    /// @brief the default value of increment value.
    static constexpr value_type default_increment_value = 1;

    /// @brief the default value of minimum value.
    static constexpr value_type default_min_value = 0;

    /// @brief the default value of maximum value.
    static constexpr value_type default_max_value = std::numeric_limits<value_type>::max();

    /// @brief the default configuration whether or not the value overflow is enabled.
    static constexpr bool default_cycle = true;

    /**
     * @brief creates a new object.
     * @param definition_id the sequence definition ID
     * @param simple_name the simple name
     * @param initial_value the initial value, should be `min_value <= initial_value <= max_value`
     * @param increment_value the increment value, must be `!= 0`
     * @param min_value the minimum value
     * @param max_value the maximum value
     * @param cycle whether or not the value overflow is enabled
     */
    explicit sequence(
            std::optional<definition_id_type> definition_id,
            simple_name_type simple_name,
            value_type initial_value = default_initial_value,
            value_type increment_value = default_increment_value,
            value_type min_value = default_min_value,
            value_type max_value = default_max_value,
            bool cycle = default_cycle) noexcept;

    /**
     * @brief creates a new object.
     * @param simple_name the simple name
     * @param initial_value the initial value, should be `min_value <= initial_value <= max_value`
     * @param increment_value the increment value, must be `!= 0`
     * @param min_value the minimum value
     * @param max_value the maximum value
     * @param cycle whether or not the value overflow is enabled
     */
    explicit sequence(
            simple_name_type simple_name,
            value_type initial_value = default_initial_value,
            value_type increment_value = default_increment_value,
            value_type min_value = default_min_value,
            value_type max_value = default_max_value,
            bool cycle = default_cycle) noexcept;

    /**
     * @brief returns the sequence definition ID.
     * @return the definition ID
     * @return empty if it is not specified
     */
    [[nodiscard]] std::optional<definition_id_type> definition_id() const noexcept;

    /**
     * @brief sets the sequence definition ID.
     * @param definition_id the definition ID
     * @return this
     */
    sequence& definition_id(std::optional<definition_id_type> definition_id) noexcept;

    /**
     * @brief returns the simple name of this table.
     * @return the simple name
     * @return empty string if it is not defined
     */
    [[nodiscard]] std::string_view simple_name() const noexcept;

    /**
     * @brief sets the simple name of this table.
     * @param simple_name the simple name
     * @return this
     */
    sequence& simple_name(simple_name_type simple_name) noexcept;

    /**
     * @brief returns the initial value of sequence.
     * @return the initial value
     */
    [[nodiscard]] value_type initial_value() const noexcept;

    /**
     * @brief sets the initial value of sequence.
     * @param value the initial value
     * @return this
     */
    sequence& initial_value(value_type value) noexcept;

    /**
     * @brief returns the increment value of sequence.
     * @return the increment value
     */
    [[nodiscard]] value_type increment_value() const noexcept;

    /**
     * @brief sets the increment value of sequence.
     * @param value the increment value
     * @return this
     */
    sequence& increment_value(value_type value) noexcept;

    /**
     * @brief returns the minimum value of sequence.
     * @return the minimum value
     */
    [[nodiscard]] value_type min_value() const noexcept;

    /**
     * @brief sets the minimum value of sequence.
     * @param value the minimum value
     * @return this
     */
    sequence& min_value(value_type value) noexcept;

    /**
     * @brief returns the maximum value of sequence.
     * @return the maximum value
     */
    [[nodiscard]] value_type max_value() const noexcept;

    /**
     * @brief sets the maximum value of sequence.
     * @param value the maximum value
     * @return this
     */
    sequence& max_value(value_type value) noexcept;

    /**
     * @brief sets whether or not value overflow is enabled.
     * @return true if the value overflow is enabled
     * @return false to be disabled if such the overflow was occurred
     */
    [[nodiscard]] bool cycle() const noexcept;

    /**
     * @brief sets whether or not value overflow is enabled.
     * When the value exceeds the value range, it will cycle to the other end point only if it is enabled.
     * Otherwise, the sequence will be disabled in such the case.
     * @param enabled whether or not value overflow is enabled.
     * @return this
     */
    sequence& cycle(bool enabled) noexcept;

    /**
     * @brief returns the owner of this sequence.
     * @return the owner
     * @return empty if there are no provides that can provide this sequence
     */
    [[nodiscard]] ::takatori::util::optional_ptr<provider const> owner() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, sequence const& value);

private:
    std::optional<definition_id_type> definition_id_ {};
    simple_name_type simple_name_;
    value_type initial_value_;
    value_type increment_value_;
    value_type min_value_;
    value_type max_value_;
    bool cycle_;

    // FIXME: ugly
    mutable provider const* owner_ {};

    friend provider;
};

/**
 * @brief returns whether or not the two sequences are same.
 * @param a the first sequence
 * @param b the second sequence
 * @return true if the both are same
 * @return false otherwise
 */
constexpr bool operator==(sequence const& a, sequence const& b) noexcept {
    return std::addressof(a) == std::addressof(b);
}

/**
 * @brief returns whether or not the two sequences are different.
 * @param a the first sequence
 * @param b the second sequence
 * @return true if the both are different
 * @return false otherwise
 */
constexpr bool operator!=(sequence const& a, sequence const& b) noexcept {
    return !(a == b);
}

} // namespace yugawara::storage
