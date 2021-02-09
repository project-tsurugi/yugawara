#pragma once

#include <numeric>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>

#include <cstdint>

#include <takatori/util/object_creator.h>

namespace yugawara::storage {

/**
 * @brief provides sequence information.
 */
class sequence {
public:
    /// @brief the schema definition ID type.
    using definition_id_type = std::size_t;

    /// @brief the simple name type.
    using simple_name_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;

    /// @brief the value type of sequences.
    using value_type = std::int64_t;

    /// @brief the default value of minimum value of the sequence.
    static constexpr value_type default_min_value = 0;

    /// @brief the default value of maximum value of the sequence.
    static constexpr value_type default_max_value = std::numeric_limits<value_type>::max();

    /// @brief the default value of minimum value of the sequence.
    static constexpr value_type default_increment_value = 1;

    /**
     * @brief creates a new object.
     * @param definition_id the sequence definition ID
     * @param simple_name the simple name
     * @param min_value the minimum value
     * @param max_value the maximum value
     * @param increment_value the increment value, should be `> 0`
     */
    explicit sequence(
            std::optional<definition_id_type> definition_id,
            simple_name_type simple_name,
            value_type min_value = default_min_value,
            value_type max_value = default_max_value,
            value_type increment_value = default_increment_value) noexcept;

    /**
     * @brief creates a new object.
     * @param simple_name the simple name
     * @param min_value the minimum value
     * @param max_value the maximum value
     * @param increment_value the increment value, should be `> 0`
     */
    explicit sequence(
            simple_name_type simple_name,
            value_type min_value = default_min_value,
            value_type max_value = default_max_value,
            value_type increment_value = default_increment_value) noexcept;

    /**
     * @brief creates a new object.
     * @param definition_id the table definition ID
     * @param simple_name the simple name
     * @param min_value the minimum value
     * @param max_value the maximum value
     * @param increment_value the increment value, should be `> 0`
     */
    explicit sequence(
            std::optional<definition_id_type> definition_id,
            std::string_view simple_name,
            value_type min_value = default_min_value,
            value_type max_value = default_max_value,
            value_type increment_value = default_increment_value) noexcept;

    /**
     * @brief creates a new object.
     * @param simple_name the simple name
     * @param min_value the minimum value
     * @param max_value the maximum value
     * @param increment_value the increment value, should be `> 0`
     */
    explicit sequence(
            std::string_view simple_name,
            value_type min_value = default_min_value,
            value_type max_value = default_max_value,
            value_type increment_value = default_increment_value) noexcept;

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
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, sequence const& value);

private:
    std::optional<definition_id_type> definition_id_ {};
    simple_name_type simple_name_;
    value_type min_value_;
    value_type max_value_;
    value_type increment_value_;
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
