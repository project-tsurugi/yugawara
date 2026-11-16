#pragma once

#include <takatori/relation/expression_kind.h>
#include <takatori/scalar/expression_kind.h>
#include <takatori/plan/step_kind.h>
#include <takatori/statement/statement_kind.h>

#include <takatori/util/maybe_shared_ptr.h>

#include "symbol_location_query.h"

namespace yugawara::remote {

/**
 * @brief options about the executable locations of operations.
 * @details This is a sub-option of compiler_options, and is available via compiler_options::location_options().
 *      The allow lists in this object declare which kinds of execution elements can be executed in locations
 *      other than the local location.
 *      Each allow list contains all kinds by default (universe), that is, the compiler assumes that
 *      the other locations can execute all kinds of elements.
 */
class location_options {
public:
    /// @brief the set type of relational operator kinds.
    using relation_expression_kind_set = ::takatori::relation::expression_kind_set;

    /// @brief the set type of scalar expression kinds.
    using scalar_expression_kind_set = ::takatori::scalar::expression_kind_set;

    /// @brief the set type of step kinds.
    using step_kind_set = ::takatori::plan::step_kind_set;

    /// @brief the set type of statement kinds.
    using statement_kind_set = ::takatori::statement::statement_kind_set;

    /**
     * @brief the default value for allowing operations which cannot be executed in any locations.
     * @see allow_empty_operation_location()
     */
    static constexpr bool default_allow_empty_operation_location = false;

    /**
     * @brief the default allow list of relational operator kinds (all kinds).
     * @see remote_relation_expression_kinds()
     */
    static constexpr relation_expression_kind_set default_remote_relation_expression_kinds =
            relation_expression_kind_set::universe();

    /**
     * @brief the default allow list of scalar expression kinds (all kinds).
     * @see remote_scalar_expression_kinds()
     */
    static constexpr scalar_expression_kind_set default_remote_scalar_expression_kinds =
            scalar_expression_kind_set::universe();

    /**
     * @brief the default allow list of step kinds (all kinds).
     * @see remote_step_kinds()
     */
    static constexpr step_kind_set default_remote_step_kinds = step_kind_set::universe();

    /**
     * @brief the default allow list of statement kinds (all kinds).
     * @see remote_statement_kinds()
     */
    static constexpr statement_kind_set default_remote_statement_kinds = statement_kind_set::universe();

    /**
     * @brief creates a new instance with default options.
     */
    location_options() noexcept = default;

    /**
     * @brief returns a symbol location query to detect where each symbol is defined.
     * @details If this is not specified, the compiler does not compute the locations of operations,
     *      and the compilation result does not provide an operation location query.
     * @return the symbol location query
     * @return empty if the symbol location query is not specified
     * @see compiler_result::operation_location_query()
     */
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<remote::symbol_location_query const>& symbol_location_query() noexcept;

    /// @copydoc symbol_location_query()
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<remote::symbol_location_query const> const& symbol_location_query() const noexcept;

    /**
     * @brief returns whether to allow operations which cannot be executed in any locations.
     * @details If this is enabled, the compiler does not report compiler_code::empty_operation_location,
     *      and the operation location query just returns empty sets for such operations.
     *      This is mainly for inspecting the compiled plans, and the SQL execution engine should not enable it.
     *      Note that compiler_options::restricted_features() are still effective even if this is enabled.
     *      This does nothing if symbol_location_query() is not specified.
     * @return true if such operations are allowed
     * @return false otherwise
     * @see compiler_result::operation_location_query()
     */
    [[nodiscard]] bool& allow_empty_operation_location() noexcept;

    /// @copydoc allow_empty_operation_location()
    [[nodiscard]] bool allow_empty_operation_location() const noexcept;

    /**
     * @brief returns the kinds of relational operators which can be executed in locations other than the local.
     * @details The default value contains all kinds.
     * @attention The compiler does not use this list yet.
     * @return the allow list of relational operator kinds
     */
    [[nodiscard]] relation_expression_kind_set& remote_relation_expression_kinds() noexcept;

    /// @copydoc remote_relation_expression_kinds()
    [[nodiscard]] relation_expression_kind_set const& remote_relation_expression_kinds() const noexcept;

    /**
     * @brief returns the kinds of scalar expressions which can be evaluated in locations other than the local.
     * @details The default value contains all kinds.
     * @attention The compiler does not use this list yet.
     * @return the allow list of scalar expression kinds
     */
    [[nodiscard]] scalar_expression_kind_set& remote_scalar_expression_kinds() noexcept;

    /// @copydoc remote_scalar_expression_kinds()
    [[nodiscard]] scalar_expression_kind_set const& remote_scalar_expression_kinds() const noexcept;

    /**
     * @brief returns the kinds of steps which can be executed in locations other than the local.
     * @details The default value contains all kinds.
     * @attention The compiler does not use this list yet.
     * @return the allow list of step kinds
     */
    [[nodiscard]] step_kind_set& remote_step_kinds() noexcept;

    /// @copydoc remote_step_kinds()
    [[nodiscard]] step_kind_set const& remote_step_kinds() const noexcept;

    /**
     * @brief returns the kinds of statements which can be executed in locations other than the local.
     * @details The default value contains all kinds.
     * @attention The compiler does not use this list yet.
     * @return the allow list of statement kinds
     */
    [[nodiscard]] statement_kind_set& remote_statement_kinds() noexcept;

    /// @copydoc remote_statement_kinds()
    [[nodiscard]] statement_kind_set const& remote_statement_kinds() const noexcept;

private:
    ::takatori::util::maybe_shared_ptr<remote::symbol_location_query const> symbol_location_query_ {};
    bool allow_empty_operation_location_ { default_allow_empty_operation_location };
    relation_expression_kind_set remote_relation_expression_kinds_ { default_remote_relation_expression_kinds };
    scalar_expression_kind_set remote_scalar_expression_kinds_ { default_remote_scalar_expression_kinds };
    step_kind_set remote_step_kinds_ { default_remote_step_kinds };
    statement_kind_set remote_statement_kinds_ { default_remote_statement_kinds };
};

} // namespace yugawara::remote
