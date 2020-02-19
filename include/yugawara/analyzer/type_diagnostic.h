#pragma once

#include <iostream>
#include <memory>
#include <utility>

#include <takatori/type/data.h>
#include <takatori/scalar/expression.h>
#include <takatori/util/enum_set.h>

#include "type_diagnostic_code.h"

#include <yugawara/type/category.h>

namespace yugawara::analyzer {

/**
 * @brief represents a diagnostic of type analysis.
 */
class type_diagnostic {
public:
    /// @brief the error code.
    using code_type = type_diagnostic_code;
    
    /// @brief the type category set.
    using category_set = ::takatori::util::enum_set<type::category, type::category::boolean, type::category::unresolved>;

    /**
     * @brief creates a new instance.
     * @param code the diagnostic code
     * @param location the location where this diagnostic occurred
     * @param actual_type the actual type
     * @param expected_categories the expected type categories
     */
    type_diagnostic(
            code_type code,
            ::takatori::scalar::expression const& location,
            std::shared_ptr<::takatori::type::data const> actual_type,
            category_set expected_categories) noexcept;

    /**
     * @brief creates a new instance.
     * @param code the diagnostic code
     * @param location the location where this diagnostic occurred
     * @param actual_type the actual type
     * @param expected_categories the expected type categories
     */
    type_diagnostic(
            code_type code,
            ::takatori::scalar::expression const& location,
            ::takatori::type::data&& actual_type,
            category_set expected_categories);
    
    /**
     * @brief returns the diagnostic code.
     * @return the diagnostic code
     */
    code_type code() const noexcept;

    /**
     * @brief returns the location where this diagnostic occurred.
     * @return the diagnostic location
     */
    ::takatori::scalar::expression const& location() const noexcept;

    /**
     * @brief returns the actual expression type.
     * @return the actual expression type
     */
    ::takatori::type::data const& actual_type() const noexcept;

    /**
     * @brief returns the actual expression type as a shared pointer.
     * @return the actual expression type
     */
    std::shared_ptr<::takatori::type::data const> shared_actual_type() const noexcept;

    /**
     * @brief returns the expected expression type categories.
     * @return the expected categories
     */
    category_set const& expected_categories() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, type_diagnostic const& value);
    
private:
    code_type code_;
    ::takatori::scalar::expression const* location_;
    std::shared_ptr<::takatori::type::data const> actual_type_;
    category_set expected_categories_;
};

} // namespace yugawara::analyzer

