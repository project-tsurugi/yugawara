#pragma once

#include <iostream>
#include <string>
#include <string_view>

#include "nullity.h"
#include "predicate.h"

#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/rvalue_ptr.h>

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
            takatori::util::unique_object_ptr<class predicate> predicate = {}) noexcept;

    /**
     * @brief creates a new object.
     * @param nullity the nullity of the target variable
     * @param predicate the structured predicate that represents invariants of the target variable
     * @attention this may take a copy of arguments
     */
    criteria(class nullity nullity, class predicate&& predicate);

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
     * @param creator the object creator
     */
    explicit criteria(criteria const& other, takatori::util::object_creator creator);

    /**
     * @brief creates a new object.
     * @param other the move source
     * @param creator the object creator
     */
    explicit criteria(criteria&& other, takatori::util::object_creator creator);

    /**
     * @brief returns the nullity of the target variable.
     * @return the nullity
     */
    class nullity nullity() const noexcept;

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
    takatori::util::optional_ptr<class predicate const> predicate() const noexcept;

    /**
     * @brief sets the structured predicate of the target variable invariant.
     * @param predicate the structured predicate
     * @return this
     */
    criteria& predicate(takatori::util::unique_object_ptr<class predicate> predicate) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output
     */
    friend std::ostream& operator<<(std::ostream& out, criteria const& value);

private:
    class nullity nullity_ { nullable };
    takatori::util::unique_object_ptr<class predicate> predicate_ {};
};

} // namespace yugawara::variable
