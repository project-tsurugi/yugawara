#pragma once

#include <memory>
#include <vector>

#include <takatori/type/data.h>
#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>
#include <takatori/statement/statement.h>
#include <takatori/serializer/object_scanner.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/sequence_view.h>

#include <yugawara/type/repository.h>
#include <yugawara/serializer/object_scanner.h>
#include <yugawara/analyzer/expression_mapping.h>
#include <yugawara/analyzer/variable_mapping.h>

#include "diagnostic.h"
#include "compiler_code.h"

namespace yugawara {

/**
 * @brief provides compiled information.
 */
class compiler_result {
public:
    /// @brief diagnostic code type.
    using code_type = compiler_code;

    /// @brief diagnostic information type.
    using diagnostic_type = diagnostic<code_type>;

    /**
     * @brief creates a new invalid instance.
     * @details This represents not a successfully result without any diagnostics.
     */
    compiler_result() = default;

    /**
     * @brief creates a new instance.
     * @param statement the compiled statement
     * @param expression_mapping information of individual expressions in the compiled statement
     * @param variable_mapping information of individual variables in the compiled statement
     */
    compiler_result(
            ::takatori::util::unique_object_ptr<::takatori::statement::statement> statement,
            std::shared_ptr<analyzer::expression_mapping const> expression_mapping,
            std::shared_ptr<analyzer::variable_mapping const> variable_mapping) noexcept;

    /**
     * @brief creates a new instance which represents a compilation error.
     * @param diagnostics diagnostics of the error
     */
    explicit compiler_result(std::vector<diagnostic_type> diagnostics) noexcept;

    /**
     * @brief returns whether or not this was successfully completed.
     * @details If this returns `false`, please check diagnostics() to address compilation errors.
     * @return true if this represents a valid compilation result
     * @return false otherwise
     */
    [[nodiscard]] bool success() const noexcept;

    /// @copydoc success()
    [[nodiscard]] explicit operator bool() const noexcept;

    /**
     * @brief returns the diagnostic information.
     * @return the diagnostic information of the compiled result.
     */
    [[nodiscard]] ::takatori::util::sequence_view<diagnostic_type const> diagnostics() const noexcept;

    /**
     * @brief returns the compiled statement.
     * @return the compiled statement
     * @warning undefined behavior if this compilation was failed
     */
    [[nodiscard]] ::takatori::statement::statement& statement() noexcept;

    /// @copydoc statement()
    [[nodiscard]] ::takatori::statement::statement const& statement() const noexcept;

    /**
     * @brief returns the compiled statement.
     * @return the compiled statement
     * @return empty if this compilation was failed
     */
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::statement::statement> optional_statement() noexcept;

    /// @see optional_statement()
    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::statement::statement const> optional_statement() const noexcept;

    /**
     * @brief detaches the compiled statement from this object.
     * @return the compiled statement
     * @return empty if this compilation was failed
     */
    [[nodiscard]] ::takatori::util::unique_object_ptr<::takatori::statement::statement> release_statement() noexcept;

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
     * @brief returns an object scanner for the compilation result.
     * @return the object scanner
     */
    [[nodiscard]] serializer::object_scanner object_scanner() const noexcept;

private:
    // successfully information
    ::takatori::util::unique_object_ptr<::takatori::statement::statement> statement_ {};
    std::shared_ptr<analyzer::expression_mapping const> expression_mapping_ {};
    std::shared_ptr<analyzer::variable_mapping const> variable_mapping_ {};

    // erroneous information
    std::vector<diagnostic_type> diagnostics_ {};
};

} // namespace yugawara
