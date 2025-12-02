#pragma once

#include <functional>
#include <memory>
#include <unordered_map>
#include <variant>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>

namespace yugawara::analyzer::details {

enum class range_hint_type {
    infinity,
    inclusive,
    exclusive,
};

class range_hint_entry {
public:
    using immediate_type = std::unique_ptr<::takatori::scalar::immediate>;

    using variable_type = ::takatori::descriptor::variable;

    using value_type = std::variant<
            std::monostate, // work in progress or infinity
            immediate_type, // immediate value
            variable_type>; // host variable

    constexpr range_hint_entry() = default;

    /**
     * @brief intersects a lower bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void intersect_lower(::takatori::scalar::immediate const& value, bool inclusive);

    /**
     * @brief intersects a lower bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void intersect_lower(variable_type const& value, bool inclusive);

    /**
     * @brief intersects an upper bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void intersect_upper(::takatori::scalar::immediate const& value, bool inclusive);

    /**
     * @brief intersects an upper bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void intersect_upper(variable_type const& value, bool inclusive);

    /**
     * @brief unifies a lower bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void union_lower(::takatori::scalar::immediate const& value, bool inclusive);

    /**
     * @brief unifies a lower bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void union_lower(variable_type const& value, bool inclusive);

    /**
     * @brief unifies an upper bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void union_upper(::takatori::scalar::immediate const& value, bool inclusive);

    /**
     * @brief unifies an upper bound value.
     * @param value the value
     * @param inclusive whether the bound is inclusive
     */
    void union_upper(variable_type const& value, bool inclusive);

    /**
     * @brief merge the other range hint into this with intersection operation.
     * @param other the other range hint
     */
    void intersect_merge(range_hint_entry&& other);

    /**
     * @brief merge the other range hint into this with union operation.
     * @param other the other range hint
     */
    void union_merge(range_hint_entry&& other);

    /**
     * @brief returns whether the range hint is empty (both ends are range_hint_type::infinity).
     * @return true if the range hint is empty
     * @return false otherwise
     */
    [[nodiscard]] bool empty() const noexcept;

    /**
     * @brief returns the lower bound type.
     * @return the lower bound type
     * @note if the lower bound is range_hint_type::infinity, lower_value() will return empty.
     */
    [[nodiscard]] range_hint_type lower_type() const noexcept;

    /**
     * @brief returns the upper bound type.
     * @return the upper bound type
     * @note if the upper bound is range_hint_type::infinity, upper_value() will return empty.
     */
    [[nodiscard]] range_hint_type upper_type() const noexcept;

    /**
     * @brief returns the lower bound value.
     * @return std::monostate if the value is absent
     * @return otherwise the lower bound value
     */
    [[nodiscard]] value_type& lower_value() noexcept;

    /**
     * @brief returns the upper bound value.
    * @return std::monostate if the value is absent
     * @return otherwise the lower bound value
     */
    [[nodiscard]] value_type& upper_value() noexcept;

private:
    range_hint_type lower_type_ { range_hint_type::infinity };
    value_type lower_value_ {};

    range_hint_type upper_type_ { range_hint_type::infinity };
    value_type upper_value_ {};

    void intersect_lower(range_hint_type type, value_type value);
    void intersect_upper(range_hint_type type, value_type value);

    void union_lower(range_hint_type type, value_type value);
    void union_upper(range_hint_type type, value_type value);
};

class range_hint_map {
public:
    using key_type = ::takatori::descriptor::variable;
    using value_type = range_hint_entry;

    using consumer_type = std::function<void(key_type const&, value_type&&)>;

    /**
     * @brief return whether the map contains the valid entry of the column.
     * @param key the targaet column
     * @return true if the map contains the valid entry
     * @return false otherwise
     */
    [[nodiscard]] bool contains(key_type const& key) const noexcept;

    /**
     * @brief returns the range hint entry for the given column.
     * If the entry does not exist, a new entry will be created.
     * @param key the target column
     * @return the range hint entry
     */
    [[nodiscard]] value_type& get(key_type const& key);

    /**
     * @brief consumes all entries in the map and pass them to the consumer function.
     * @param consumer the consumer function
     */
    void consume(consumer_type const& consumer);

    /**
     * @brief merge the other range hint entries into this with intersection operation.
     * @param other the other range hint map
     */
    void intersect_merge(range_hint_map&& other);

    /**
     * @brief merge the other range hint entries into this with union operation.
     * @param other the other range hint map
     */
    void union_merge(range_hint_map&& other);

private:
    std::unordered_map<key_type, value_type> entries_;
};

} // namespace yugawara::analyzer::details
