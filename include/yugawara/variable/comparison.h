#pragma once

#include <iostream>
#include <memory>

#include <takatori/value/data.h>
#include <takatori/scalar/comparison_operator.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>

#include "predicate.h"
#include "predicate_kind.h"

namespace yugawara::variable {

/**
 * @brief represents kind of comparison operators.
 */
using takatori::scalar::comparison_operator;

/**
 * @brief a predicate that compares variable with a value using comparison operator.
 */
class comparison : public predicate {
public:
    /// @brief comparison operator kind.
    using operator_kind_type = comparison_operator;

    /// @brief the kind of this predicate.
    static constexpr inline predicate_kind tag = predicate_kind::comparison;

    /**
     * @brief creates a new object.
     * @param operator_kind the comparison operator
     * @param value the comparative value (right hand side of comparison)
     */
    explicit comparison(
            operator_kind_type operator_kind,
            std::shared_ptr<takatori::value::data const> value) noexcept;

    /**
     * @brief creates a new object.
     * @param operator_kind the comparison operator
     * @param value the comparative value
     * @attention this may take a copy of arguments
     */
    explicit comparison(
            operator_kind_type operator_kind,
            takatori::value::data&& value);

    /**
     * @brief creates a new object.
     * @param other the copy source
     * @param creator the object creator
     */
    explicit comparison(comparison const& other, takatori::util::object_creator creator) noexcept;

    /**
     * @brief creates a new object.
     * @param other the move source
     * @param creator the object creator
     */
    explicit comparison(comparison&& other, takatori::util::object_creator creator) noexcept;

    predicate_kind kind() const noexcept override;
    comparison* clone(takatori::util::object_creator creator) const& override;
    comparison* clone(takatori::util::object_creator creator) && override;

    /**
     * @brief returns the operator kind.
     * @return operator kind
     */
    operator_kind_type operator_kind() const noexcept;

    /**
     * @brief sets operator kind.
     * @param operator_kind operator kind
     * @return this
     */
    comparison& operator_kind(operator_kind_type operator_kind) noexcept;

    /**
     * @brief returns the comparative value.
     * @return the comparative value
     * @warning undefined behavior if the value is absent
     */
    takatori::value::data const& value() const noexcept;

    /**
     * @brief returns the comparative value.
     * @return the comparative value
     * @return empty if the value is absent
     */
    takatori::util::optional_ptr<takatori::value::data const> optional_value() const noexcept;

    /**
     * @brief returns the comparative value for share its value.
     * @return the comparative value for sharing
     * @return empty if the value is absent
     */
    std::shared_ptr<takatori::value::data const> shared_value() const noexcept;

    /**
     * @brief sets a comparative value.
     * @param value the comparative value
     * @return this
     */
    comparison& value(std::shared_ptr<takatori::value::data const> value) noexcept;

    /**
     * @brief returns whether or not the two elements are equivalent.
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(comparison const& a, comparison const& b) noexcept;

    /**
     * @brief returns whether or not the two elements are different.
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(comparison const& a, comparison const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, comparison const& value);

protected:
    bool equals(predicate const& other) const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    operator_kind_type operator_kind_;
    std::shared_ptr<takatori::value::data const> value_;
};

} // namespace yugawara::variable
