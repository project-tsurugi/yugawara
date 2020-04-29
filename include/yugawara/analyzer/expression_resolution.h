#pragma once

#include <memory>

#include <takatori/type/data.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/type/repository.h>

namespace yugawara::analyzer {

/**
 * @brief represents information of resolved expressions.
 */
class expression_resolution {
public:
    /**
     * @brief creates a new unresolved instance.
     */
    constexpr expression_resolution() = default;

    /**
     * @brief creates a new instance.
     * @param type the resolved expression type
     */
    expression_resolution(std::shared_ptr<::takatori::type::data const> type) noexcept; // NOLINT

    /**
     * @brief creates a new instance.
     * @param type the resolved expression type
     * @param repo the type repository
     * @attention this may copy the given input type
     */
    expression_resolution( // NOLINT
            ::takatori::type::data&& type,
            type::repository& repo = type::default_repository());

    /**
     * @brief returns the resolved type.
     * @return the resolved type
     * @throws std::domain_error if this is unresolved
     */
    [[nodiscard]] ::takatori::type::data const& type() const;

    /**
     * @brief returns the resolved type.
     * @return the resolved type
     * @return empty if this is unresolved
     */
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::type::data const> optional_type() const noexcept;

    /**
     * @brief returns the resolved type as shared pointer.
     * @return the resolved type
     * @return empty if this is unresolved
     */
    [[nodiscard]] std::shared_ptr<::takatori::type::data const> shared_type() const noexcept;

    /**
     * @brief returns whether or not this represents resolved information.
     * @return true if this represents resolved information
     * @return false if this is unresolved information
     */
    [[nodiscard]] bool is_resolved() const noexcept;

    /// @copydoc is_resolved()
    [[nodiscard]] explicit operator bool() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, expression_resolution const& value);

private:
    std::shared_ptr<::takatori::type::data const> type_;

    void check_resolved() const;
};

/**
 * @brief returns whether or not the two resolution information is equivalent.
 * @param a the first element
 * @param b the second element
 * @return true if the both are equivalent
 * @return false otherwise
 */
bool operator==(expression_resolution const& a, expression_resolution const& b);

/**
 * @brief returns whether or not the two resolution information is different.
 * @param a the first element
 * @param b the second element
 * @return true if the both are different
 * @return false otherwise
 */
bool operator!=(expression_resolution const& a, expression_resolution const& b);

} // namespace yugawara::analyzer
