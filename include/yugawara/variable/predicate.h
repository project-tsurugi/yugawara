#pragma once

#include <ostream>

#include <takatori/util/object_creator.h>

#include "predicate_kind.h"

namespace yugawara::variable {

/**
 * @brief an abstract interface of structured predicates.
 */
class predicate {
public:
    /**
     * @brief destroys this object.
     */
    virtual ~predicate() = default;

    /**
     * @brief returns the kind of this value.
     * @return the value kind
     */
    [[nodiscard]] virtual predicate_kind kind() const noexcept = 0;

    /**
     * @brief returns a clone of this object.
     * @param creator the object creator
     * @return the created clone
     */
    [[nodiscard]] virtual predicate* clone(takatori::util::object_creator creator) const& = 0;

    /// @copydoc clone()
    [[nodiscard]] virtual predicate* clone(takatori::util::object_creator creator) && = 0;

    /**
     * @brief returns whether or not the two elements are equivalent.
     * @attention This only compares their syntactic form, that is,
     *      two different expressions are not equivalent even if there are tautology
     *      (e.g. `A & B` is differ from `B & A`)
     * @param a the first element
     * @param b the second element
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(predicate const& a, predicate const& b) noexcept;

    /**
     * @brief returns whether or not the two elements are different.
     * @attention This only compares their syntactic form, that is,
     *      two different expressions are not equivalent even if there are tautology
     *      (e.g. `A & B` is differ from `B & A`)
     * @param a the first element
     * @param b the second element
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(predicate const& a, predicate const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, predicate const& value);

protected:
    /**
     * @brief creates a new instance.
     */
    predicate() = default;

    /**
     * @brief creates a new instance.
     * @param other the copy source
     */
    predicate(predicate const& other) = default;

    /**
     * @brief assigns the given object.
     * @param other the copy source
     * @return this
     */
    predicate& operator=(predicate const& other) = default;

    /**
     * @brief creates a new instance.
     * @param other the move source
     */
    predicate(predicate&& other) noexcept = default;

    /**
     * @brief assigns the given object.
     * @param other the move source
     * @return this
     */
    predicate& operator=(predicate&& other) noexcept = default;

    /**
     * @brief returns whether or not this type is equivalent to the target one.
     * @param other the target type
     * @return true if the both are equivalent
     * @return false otherwise
     */
    [[nodiscard]] virtual bool equals(predicate const& other) const noexcept = 0;

    /**
     * @brief appends string representation of this object into the given output.
     * @param out the target output
     * @return the output
     */
    virtual std::ostream& print_to(std::ostream& out) const = 0;
};

} // namespace yugawara::variable
