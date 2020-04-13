#pragma once

#include <iostream>
#include <memory>
#include <utility>

#include <takatori/document/region.h>
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

    /**
     * @brief creates a new instance.
     * @param code the diagnostic code
     * @param region the region where this diagnostic occurred
     * @param actual_type the actual type
     * @param expected_categories the expected type categories
     */
    type_diagnostic(
            code_type code,
            ::takatori::document::region region,
            std::shared_ptr<::takatori::type::data const> actual_type,
            type::category_set expected_categories) noexcept;

    /**
     * @brief creates a new instance.
     * @param code the diagnostic code
     * @param region the region where this diagnostic occurred
     * @param actual_type the actual type
     * @param expected_categories the expected type categories
     */
    type_diagnostic(
            code_type code,
            ::takatori::document::region region,
            ::takatori::type::data&& actual_type,
            type::category_set expected_categories);
    
    /**
     * @brief returns the diagnostic code.
     * @return the diagnostic code
     */
    [[nodiscard]] code_type code() const noexcept;

    /**
     * @brief returns the region where this diagnostic occurred.
     * @return the diagnostic region
     */
    [[nodiscard]] ::takatori::document::region const& region() const noexcept;

    /**
     * @brief returns the actual expression type.
     * @return the actual expression type
     */
    [[nodiscard]] ::takatori::type::data const& actual_type() const noexcept;

    /**
     * @brief returns the actual expression type as a shared pointer.
     * @return the actual expression type
     */
    [[nodiscard]] std::shared_ptr<::takatori::type::data const> shared_actual_type() const noexcept;

    /**
     * @brief returns the expected expression type categories.
     * @return the expected categories
     */
    [[nodiscard]] type::category_set const& expected_categories() const noexcept;

    /**
     * @brief appends string representation of the given value.
     * @param out the target output
     * @param value the target value
     * @return the output stream
     */
    friend std::ostream& operator<<(std::ostream& out, type_diagnostic const& value);
    
private:
    code_type code_;
    ::takatori::document::region region_;
    std::shared_ptr<::takatori::type::data const> actual_type_;
    type::category_set expected_categories_;
};

} // namespace yugawara::analyzer

