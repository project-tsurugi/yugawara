#pragma once

#include "variable_info.h"
#include "variable_info_kind.h"

#include <yugawara/variable/declaration.h>

namespace yugawara::binding {

/**
 * @brief refers existing external variables.
 */
class external_variable_info : public variable_info {
public:
    /// @brief the variable kind.
    static constexpr variable_info_kind tag = variable_info_kind::external_variable;

    /**
     * @brief creates a new object.
     * @param declaration the original declaration
     */
    explicit external_variable_info(variable::declaration const& declaration) noexcept;

    [[nodiscard]] variable_info_kind kind() const noexcept override;

    /**
     * @brief returns the original declaration.
     * @return the original declaration
     */
    [[nodiscard]] variable::declaration const& declaration() const noexcept;

    /**
     * @brief returns whether or not the two relations are equivalent.
     * @param a the first relation
     * @param b the second relation
     * @return true if a == b
     * @return false otherwise
     */
    friend bool operator==(external_variable_info const& a, external_variable_info const& b) noexcept;

    /**
     * @brief returns whether or not the two relations are different.
     * @param a the first relation
     * @param b the second relation
     * @return true if a != b
     * @return false otherwise
     */
    friend bool operator!=(external_variable_info const& a, external_variable_info const& b) noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, external_variable_info const& value);

protected:
    [[nodiscard]] bool equals(variable_info const& other) const noexcept override;
    [[nodiscard]] std::size_t hash() const noexcept override;
    std::ostream& print_to(std::ostream& out) const override;

private:
    variable::declaration const* declaration_;
};

} // namespace yugawara::binding
