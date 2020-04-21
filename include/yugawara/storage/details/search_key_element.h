#pragma once

#include <iostream>

#include <takatori/scalar/expression.h>
#include <takatori/relation/endpoint_kind.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/storage/column.h>

namespace yugawara::storage::details {

/**
 * @brief search criteria of individual index key columns.
 */
class search_key_element {
public:
    /**
     * @brief creates a new instance for the equivalence criteria.
     * @param column the target column
     * @param value the equivalent expression
     */
    search_key_element(
            class column const& column,
            ::takatori::scalar::expression const& value) noexcept;

    /**
     * @brief creates a new instance for the 1-dimensional range criteria.
     * @param column the target column
     * @param lower_value the lower bound value (nullable)
     * @param lower_inclusive whether or not the lower bound is inclusive
     * @param upper_value the upper bound value (nullable)
     * @param upper_inclusive whether or not the upper bound is inclusive
     */
    search_key_element(
            class column const& column,
            ::takatori::util::optional_ptr<::takatori::scalar::expression const> lower_value,
            bool lower_inclusive,
            ::takatori::util::optional_ptr<::takatori::scalar::expression const> upper_value,
            bool upper_inclusive) noexcept;

    /**
     * @brief returns the target column.
     * @return the target column
     */
    [[nodiscard]] class column const& column() const noexcept;

    /**
     * @brief returns whether or not the lower and upper bound is equivalent and both inclusive.
     * @return true if the both bounds are equivalent
     * @return false if the both bounds represent a 1-dimensional range
     */
    [[nodiscard]] bool equivalent() const;

    /**
     * @brief returns whether or not both the lower and upper bound exists.
     * @return true the both bound exists
     * @return false otherwise
     */
    [[nodiscard]] bool bounded() const;

    /**
     * @brief returns the equivalent value.
     * @return the equivalent value
     * @return empty if the equivalent is unbound
     */
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::scalar::expression const> equivalent_value() const;

    /**
     * @brief returns the lower bound value.
     * @return the lower bound value
     * @return empty if the lower is unbound
     */
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::scalar::expression const> lower_value() const;

    /**
     * @brief returns whether or not the lower bound is inclusive.
     * @return true if the lower bound is inclusive for the lower_value()
     * @return false if lower bound is exclusive for the lower_value()
     */
    [[nodiscard]] bool lower_inclusive() const;

    /**
     * @brief returns the upper bound value.
     * @return the upper bound value
     * @return empty if the upper is unbound
     */
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::scalar::expression const> upper_value() const;

    /**
     * @brief returns whether or not the upper bound is inclusive.
     * @return true if the upper bound is inclusive for the upper_value()
     * @return false if upper bound is exclusive for the upper_value()
     */
    [[nodiscard]] bool upper_inclusive() const;
    
private:
    class column const* column_;
    ::takatori::util::optional_ptr<::takatori::scalar::expression const> equivalent_value_ {};
    ::takatori::util::optional_ptr<::takatori::scalar::expression const> lower_value_ {};
    bool lower_inclusive_ {};
    ::takatori::util::optional_ptr<::takatori::scalar::expression const> upper_value_ {};
    bool upper_inclusive_ {};
};

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
std::ostream& operator<<(std::ostream& out, search_key_element const& value);

} // namespace yugawara::storage::details