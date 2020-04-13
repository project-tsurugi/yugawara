#pragma once

#include <initializer_list>
#include <iostream>
#include <memory>

#include <takatori/scalar/quantifier.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/reference_list_view.h>
#include <takatori/util/reference_vector.h>
#include <takatori/util/rvalue_reference_wrapper.h>

#include "predicate.h"
#include "predicate_kind.h"

namespace yugawara::variable {

using ::takatori::scalar::quantifier;

/**
 * @brief a quantification of predicates.
 */
class quantification : public predicate {
public:
    /// @brief the operator kind type.
    using operator_kind_type = quantifier;

    /// @brief the kind of this predicate.
    static constexpr inline predicate_kind tag = predicate_kind::quantification;

    /**
     * @brief creates a new object.
     * @param operator_kind the quantifier
     * @param operands the quantification operands
     */
    explicit quantification(
            operator_kind_type operator_kind,
            takatori::util::reference_vector<predicate> operands) noexcept;

    /**
     * @brief creates a new object.
     * @param operator_kind the quantifier
     * @param left the left term
     * @param right the right term
     * @attention this may take a copy of arguments
     */
    explicit quantification(
            operator_kind_type operator_kind,
            predicate&& left,
            predicate&& right);

    /**
     * @brief creates a new object.
     * @param operator_kind the quantifier
     * @param operands the quantification operands
     * @attention this may take a copy of arguments
     */
    explicit quantification(
            operator_kind_type operator_kind,
            std::initializer_list<takatori::util::rvalue_reference_wrapper<predicate>> operands);

    /**
     * @brief creates a new object.
     * @param other the copy source
     * @param creator the object creator
     */
    explicit quantification(quantification const& other, takatori::util::object_creator creator) noexcept;

    /**
     * @brief creates a new object.
     * @param other the move source
     * @param creator the object creator
     */
    explicit quantification(quantification&& other, takatori::util::object_creator creator) noexcept;

    [[nodiscard]] predicate_kind kind() const noexcept override;
    [[nodiscard]] quantification* clone(takatori::util::object_creator creator) const& override;
    [[nodiscard]] quantification* clone(takatori::util::object_creator creator) && override;

    /**
     * @brief returns the operator kind.
     * @return operator kind
     */
    [[nodiscard]] operator_kind_type operator_kind() const noexcept;

    /**
     * @brief sets operator kind.
     * @param operator_kind operator kind
     * @return this
     */
    quantification& operator_kind(operator_kind_type operator_kind) noexcept;

    /**
     * @brief returns the operands.
     * @return the operands
     */
    [[nodiscard]] takatori::util::reference_vector<predicate>& operands() noexcept;

    /// @copydoc operands()
    [[nodiscard]] takatori::util::reference_vector<predicate> const& operands() const noexcept;

    /**
     * @brief returns whether or not the two elements are equivalent.
     * @attention This only compares their syntactic form, that is,
     *      two different representations are not equivalent even if there are tautology
     *      (e.g. `A & B` is differ from `B & A`)
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(quantification const& a, quantification const& b) noexcept;

    /**
     * @brief returns whether or not the two elements are different.
     * @attention This only compares their syntactic form, that is,
     *      two different representations are not equivalent even if there are tautology
     *      (e.g. `A & B` is differ from `B & A`)
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(quantification const& a, quantification const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, quantification const& value);

protected:
    [[nodiscard]] bool equals(predicate const& other) const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    operator_kind_type operator_kind_;
    takatori::util::reference_vector<predicate> operands_;
};

} // namespace yugawara::variable
