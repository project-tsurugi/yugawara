#pragma once

#include <vector>

#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/object_creator.h>

#include "declaration.h"

namespace yugawara::schema {

/**
 * @brief represents a search path of schemas.
 */
class search_path {
public:
    /// @brief the element type.
    using element_type = declaration const;

    /// @brief the pointer type.
    using pointer = ::takatori::util::maybe_shared_ptr<element_type>;

    /// @brief the vector of element pointer type.
    using vector_type = std::vector<pointer, ::takatori::util::object_allocator<pointer>>;

    /**
     * @brief creates a new instance.
     * @param elements the search path elements
     * @note the analyzer will search for elements from schema from head of the elements
     */
    explicit search_path(vector_type elements) noexcept;

    /// @copydoc search_path(vector_type)
    search_path(std::initializer_list<pointer> elements);

    /**
     * @brief returns the search path elements.
     * @return the search path elements
     */
    [[nodiscard]] vector_type& elements() noexcept;

    /**
     * @brief returns the search path elements.
     * @return the search path elements
     */
    [[nodiscard]] vector_type const& elements() const noexcept;

private:
    vector_type elements_;
};

/**
 * @brief compares two values.
 * @param a the first value
 * @param b the second value
 * @return true if the both has same sequence of schemas
 * @return false otherwise
 */
[[nodiscard]] bool operator==(search_path const& a, search_path const& b) noexcept;

/**
 * @brief compares two values.
 * @param a the first value
 * @param b the second value
 * @return true if the both are different
 * @return false otherwise
 */
[[nodiscard]] bool operator!=(search_path const& a, search_path const& b) noexcept;

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output stream
 */
std::ostream& operator<<(std::ostream& out, search_path const& value);

} // namespace yugawara::schema
