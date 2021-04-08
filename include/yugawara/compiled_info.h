#pragma once

#include <memory>
#include <vector>

#include <takatori/type/data.h>
#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>
#include <takatori/serializer/object_scanner.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/serializer/object_scanner.h>
#include <yugawara/analyzer/expression_mapping.h>
#include <yugawara/analyzer/variable_mapping.h>

namespace yugawara {

/**
 * @brief provides analyzed information.
 */
class compiled_info {
public:
    /**
     * @brief creates a new instance which provides no information.
     */
    compiled_info() = default;

    /**
     * @brief creates a new instance.
     * @param expression_mapping information of individual expressions
     * @param variable_mapping information of individual variables
     */
    compiled_info(
            std::shared_ptr<analyzer::expression_mapping const> expression_mapping,
            std::shared_ptr<analyzer::variable_mapping const> variable_mapping) noexcept;

    // FIXME: nullity and more

    /**
     * @brief returns the type of the given expression.
     * @details the target expression must appear in the compilation result.
     * @param expression the target expression
     * @return the expression type
     * @throws std::invalid_argument if the given expression is not a member of the compilation result
     * @warning undefined behavior if this compiled information is not valid
     */
    [[nodiscard]] ::takatori::type::data const& type_of(::takatori::scalar::expression const& expression) const;

    /**
     * @brief returns the type of the given variable.
     * @details the target variable must appear in the compilation result.
     * @param variable the target variable
     * @return the variable type
     * @throws std::invalid_argument if the given variable is not a member of the compilation result
     * @warning undefined behavior if this compiled information is not valid
     */
    [[nodiscard]] ::takatori::type::data const& type_of(::takatori::descriptor::variable const& variable) const;

    /**
     * @brief returns the all expressions analyzed by the compiler.
     * @return the expression mappings
     */
    [[nodiscard]] analyzer::expression_mapping const& expressions() const noexcept;

    /**
     * @brief returns the all variables analyzed by the compiler.
     * @return the variable mappings
     */
    [[nodiscard]] analyzer::variable_mapping const& variables() const noexcept;

    /**
     * @brief returns an object scanner for the compilation result.
     * @return the object scanner
     */
    [[nodiscard]] serializer::object_scanner object_scanner() const noexcept;

private:
    std::shared_ptr<analyzer::expression_mapping const> expression_mapping_ {};
    std::shared_ptr<analyzer::variable_mapping const> variable_mapping_ {};
};

} // namespace yugawara
