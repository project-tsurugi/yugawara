#pragma once

#include <memory>
#include <ostream>
#include <string>
#include <string_view>

#include <takatori/value/data.h>
#include <takatori/util/clone_tag.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/rvalue_ptr.h>

#include "nullity.h"
#include "predicate.h"

namespace yugawara::variable {

/**
 * @brief criteria of variables.
 */
class criteria {
public:
    /**
     * @brief creates a new object.
     * @param nullity the nullity of the target variable
     * @param predicate the structured predicate that represents invariants of the target variable
     */
    criteria( // NOLINT
            class nullity nullity = nullable,
            std::unique_ptr<class predicate> predicate = {}) noexcept;

    /**
     * @brief creates a new object.
     * @param nullity the nullity of the target variable
     * @param predicate the structured predicate that represents invariants of the target variable
     * @attention this may take a copy of arguments
     */
    criteria(class nullity nullity, class predicate&& predicate);

    /**
     * @brief creates a new object which represents a "constant criteria".
     * @param constant the constant value
     */
    criteria(takatori::value::data&& constant); // NOLINT

    ~criteria() = default;

    criteria(criteria const& other) = delete;
    criteria& operator=(criteria const& other) = delete;

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    criteria(criteria&& other) noexcept = default;

    /**
     * @brief assigns the given object.
     * @param other the move source
     * @return this
     */
    criteria& operator=(criteria&& other) noexcept = default;

    /**
     * @brief creates a new object.
     * @param other the copy source
     */
    explicit criteria(::takatori::util::clone_tag_t, criteria const& other);

    /**
     * @brief creates a new object.
     * @param other the move source
     */
    explicit criteria(::takatori::util::clone_tag_t, criteria&& other);

    /**
     * @brief returns the nullity of the target variable.
     * @return the nullity
     */
    [[nodiscard]] class nullity nullity() const noexcept;

    /**
     * @brief sets the nullity of the target variable.
     * @param nullity the nullity
     * @return this
     */
    criteria& nullity(class nullity nullity) noexcept;

    /**
     * @brief returns the structured predicate that represents the invariant of the target variable.
     * @return the structured predicate
     * @return empty if it is absent, which represents there are no special invariants
     */
    [[nodiscard]] takatori::util::optional_ptr<class predicate const> predicate() const noexcept;

    /**
     * @brief sets the structured predicate of the target variable invariant.
     * @param predicate the structured predicate
     * @return this
     */
    criteria& predicate(std::unique_ptr<class predicate> predicate) noexcept;

    /**
     * @brief returns the constant value of the variable.
     * @return the constant value
     * @return empty if the variable is not decided as a constant
     */
    [[nodiscard]] takatori::util::optional_ptr<takatori::value::data const> constant() const noexcept;

    /**
     * @brief returns the constant value of the variable as its shared pointer.
     * @return the constant value as its shared pointer
     * @return empty if the variable is not decided as a constant
     */
    [[nodiscard]] std::shared_ptr<takatori::value::data const> const& shared_constant() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, criteria const& value);

private:
    class nullity nullity_ { nullable };
    std::unique_ptr<class predicate> predicate_ {};
};

/**
 * @brief returns whether or not the two criteria are equivalent.
 * @param a the first criteria
 * @param b the second criteria
 * @return true if the both are equivalent
 * @return false otherwise
 */
bool operator==(criteria const& a, criteria const& b) noexcept;

/**
 * @brief returns whether or not the two criteria are different.
 * @param a the first criteria
 * @param b the second criteria
 * @return true if the both are different
 * @return false otherwise
 */
bool operator!=(criteria const& a, criteria const& b) noexcept;

} // namespace yugawara::variable
