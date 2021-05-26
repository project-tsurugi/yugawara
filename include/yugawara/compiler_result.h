#pragma once

#include <memory>
#include <vector>

#include <takatori/type/data.h>
#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>
#include <takatori/statement/statement.h>
#include <takatori/serializer/object_scanner.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/sequence_view.h>

#include <yugawara/type/repository.h>
#include <yugawara/serializer/object_scanner.h>
#include <yugawara/analyzer/expression_mapping.h>
#include <yugawara/analyzer/variable_mapping.h>

#include "diagnostic.h"
#include "compiler_code.h"
#include "compiled_info.h"

namespace yugawara {

/**
 * @brief provides compiled information.
 */
class compiler_result {
public:
    /// @brief the compiled information type.
    using info_type = compiled_info;

    /// @brief diagnostic code type.
    using code_type = compiler_code;

    /// @brief diagnostic information type.
    using diagnostic_type = diagnostic<code_type>;

    /**
     * @brief creates a new invalid instance.
     * @details This represents erroneous result without any diagnostics.
     */
    compiler_result() = default;

    /**
     * @brief creates a new instance.
     * @param statement the compiled statement
     * @param info the compiled information
     */
    compiler_result(
            std::unique_ptr<::takatori::statement::statement> statement,
            info_type info) noexcept;

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
    [[nodiscard]] std::unique_ptr<::takatori::statement::statement> release_statement() noexcept;

    /**
     * @brief returns the compiled information.
     * @return the compiled information
     * @return empty information if compilation was failed
     * @note You can take a copy of the returned object at low cost.
     */
    [[nodiscard]] info_type& info() noexcept;

    /// @copydoc info()
    [[nodiscard]] info_type const& info() const noexcept;

    /**
     * @brief returns the diagnostic information.
     * @return the diagnostic information of the compiled result.
     */
    [[nodiscard]] ::takatori::util::sequence_view<diagnostic_type const> diagnostics() const noexcept;

    /**
     * @copydoc compiled_info::type_of(::takatori::scalar::expression const&) const
     * @note This is equivalent to `info().type_of(expression)`.
     */
    [[nodiscard]] ::takatori::type::data const& type_of(::takatori::scalar::expression const& expression) const;

    /**
     * @copydoc compiled_info::type_of(::takatori::descriptor::variable const&) const
     * @note This is equivalent to `info().type_of(variable)`.
     */
    [[nodiscard]] ::takatori::type::data const& type_of(::takatori::descriptor::variable const& variable) const;

    /**
     * @copydoc compiled_info::object_scanner() const
     * @note This is equivalent to `info().object_scanner()`.
     */
    [[nodiscard]] serializer::object_scanner object_scanner() const noexcept;

private:
    // successfully information
    std::unique_ptr<::takatori::statement::statement> statement_ {};
    info_type info_ {};

    // erroneous information
    std::vector<diagnostic_type> diagnostics_ {};
};

} // namespace yugawara
