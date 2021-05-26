#pragma once

#include <memory>
#include <ostream>

#include <takatori/util/clone_tag.h>
#include <takatori/util/optional_ptr.h>

#include "predicate.h"
#include "predicate_kind.h"

namespace yugawara::variable {

/**
 * @brief a negation of predicates.
 */
class negation : public predicate {
public:
    /// @brief the kind of this predicate.
    static constexpr inline predicate_kind tag = predicate_kind::negation;

    /**
     * @brief creates a new object.
     * @param operand the negation target
     */
    explicit negation(std::unique_ptr<predicate> operand) noexcept;

    /**
     * @brief creates a new object.
     * @param operand the negation target
     * @attention this may take a copy of argument
     */
    explicit negation(predicate&& operand);

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit negation(::takatori::util::clone_tag_t, negation const& other) noexcept;

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit negation(::takatori::util::clone_tag_t, negation&& other) noexcept;

    [[nodiscard]] predicate_kind kind() const noexcept override;
    [[nodiscard]] negation* clone() const& override;
    [[nodiscard]] negation* clone() && override;

    /**
     * @brief returns the negation target.
     * @return the negation target
     * @warning undefined behavior if the operand is absent
     */
    [[nodiscard]] predicate& operand() noexcept;

    /**
     * @brief returns the negation target.
     * @return the negation target
     * @warning undefined behavior if the operand is absent
     */
    [[nodiscard]] predicate const& operand() const noexcept;

    /**
     * @brief returns the negation target.
     * @return the negation target
     * @return empty if the operand is absent
     */
    [[nodiscard]] takatori::util::optional_ptr<predicate> optional_operand() noexcept;

    /// @copydoc optional_operand()
    [[nodiscard]] takatori::util::optional_ptr<predicate const> optional_operand() const noexcept;

    /**
     * @brief releases the negation target.
     * @return the negation target
     * @return empty if the operand is absent
     */
    [[nodiscard]] std::unique_ptr<predicate> release_operand() noexcept;

    /**
     * @brief sets the negation target.
     * @param operand the replacement
     * @return this
     */
    negation& operand(std::unique_ptr<predicate> operand) noexcept;

    /**
     * @brief returns whether or not the two elements are equivalent.
     * @attention This only compares their syntactic form, that is,
     *      two different representations are not equivalent even if there are tautology
     *      (e.g. `~A` is differ from `~(A & B)`)
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(negation const& a, negation const& b) noexcept;

    /**
     * @brief returns whether or not the two elements are different.
     * @attention This only compares their syntactic form, that is,
     *      two different representations are not equivalent even if there are tautology
     *      (e.g. `~A` is differ from `~(A & B)`)
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(negation const& a, negation const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, negation const& value);

protected:
    [[nodiscard]] bool equals(predicate const& other) const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    std::unique_ptr<predicate> operand_;
};

} // namespace yugawara::variable
