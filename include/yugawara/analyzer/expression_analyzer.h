#pragma once

#include <memory>
#include <vector>

#include <takatori/descriptor/variable.h>
#include <takatori/statement/statement.h>
#include <takatori/plan/step.h>
#include <takatori/plan/graph.h>
#include <takatori/relation/expression.h>
#include <takatori/relation/graph.h>
#include <takatori/scalar/expression.h>
#include <takatori/type/data.h>
#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/sequence_view.h>

#include "expression_analyzer_code.h"
#include "expression_mapping.h"
#include "variable_mapping.h"

#include <yugawara/diagnostic.h>
#include <yugawara/type/repository.h>

namespace yugawara::analyzer {

/**
 * @brief computes each type of expressions.
 */
class expression_analyzer {
public:
    /// @brief the analyzer diagnostic type.
    using diagnostic_type = diagnostic<expression_analyzer_code>;

    /**
     * @brief creates a new instance.
     */
    expression_analyzer();

    /**
     * @brief creates a new instance.
     * @param expressions the expression mapping
     * @param variables the variable mapping
     */
    expression_analyzer(
            ::takatori::util::maybe_shared_ptr<expression_mapping> expressions,
            ::takatori::util::maybe_shared_ptr<variable_mapping> variables) noexcept;

    /**
     * @brief returns the underlying expression mapping.
     * @return the underlying expression mapping.
     */
    [[nodiscard]] expression_mapping& expressions() noexcept;

    /// @copydoc expressions()
    [[nodiscard]] expression_mapping const& expressions() const noexcept;

    /// @copydoc expressions()
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<expression_mapping> shared_expressions() noexcept;

    /**
     * @brief returns the underlying variable mapping.
     * @return the underlying variable mapping.
     */
    [[nodiscard]] variable_mapping& variables() noexcept;

    /// @copydoc variables()
    [[nodiscard]] variable_mapping const& variables() const noexcept;

    /// @copydoc variables()
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<variable_mapping> shared_variables() noexcept;

    /**
     * @brief whether or not this allow some unresolved elements in validation.
     * @return true if this allow some unresolved elements
     * @return false otherwise
     */
    [[nodiscard]] bool allow_unresolved() const noexcept;

    /**
     * @brief sets whether or not this allows some unresolved elements in validation.
     * @param allow whether or not this allows some unresolved elements (default: true)
     * @return this
     */
    expression_analyzer& allow_unresolved(bool allow) noexcept;

    /**
     * @brief extracts the type of the given resolution.
     * @param resolution the target resolution
     * @return the resolved type, may be an erroneous or pending type
     * @return empty if it includes unresolved elements
     */
    [[nodiscard]] std::shared_ptr<::takatori::type::data const> inspect(variable_resolution const& resolution) const;

    /**
     * @brief returns the type of the given variable.
     * @param variable the target variable
     * @return the resolved type, may be an erroneous or pending type
     * @return empty if it is not yet resolved
     */
    [[nodiscard]] std::shared_ptr<::takatori::type::data const> inspect(::takatori::descriptor::variable const& variable) const;

    /**
     * @brief computes the type of the given scalar expression.
     * @details the including variables must be resolved before this operation, or this will return an erroneous type.
     *      Note that, this also computes sub-expressions of the given expression.
     * @param expression the target expression
     * @param validate whether or not the operation validates the all sub-elements,
     *      if `false`, this computes the sub-elements only for decide expression types
     * @param repo the type repository
     * @return the resolved type if resolution was succeeded
     * @return erroneous type if the expression was wrong
     * @return pending type if the input contains erroneous or pending type and the resulting type will refer its input
     * @throws std::domain_error if the specified expression contains unresolved variables
     * @note this operation may add diagnostics() if the expression was wrong, but the resulting type will not be
     *      an erroneous type (e.g. the resulting type does not depends on the input expressions)
     * @note if resolution was not success, this may return some special types generated out of the given repository
     */
    std::shared_ptr<::takatori::type::data const> resolve(
            ::takatori::scalar::expression const& expression,
            bool validate = false,
            type::repository& repo = type::default_repository());

    /**
     * @brief computes the type of variables in the relational expression.
     * @details this also computes types of scalar expressions in the given expression only if they are needed to compute
     *      types of declared variables in the expression.
     * @param validate whether or not the operation validates the all sub-elements,
     *      if `false`, this computes the sub-elements only for decide the variable types
     * @param recursive if true, this also resolves the upstream relational expressions.
     *      Or never resolves the upstream relational expressions, that is, each variable which is referred from
     *      the expression must be resolved before this operation.
     * @param expression the target expression
     * @param repo the type repository
     * @return true if resolution was success
     * @return false if resolution was not success, please refer diagnostics()
     */
    bool resolve(
            ::takatori::relation::expression const& expression,
            bool validate = false,
            bool recursive = false,
            type::repository& repo = type::default_repository());

    /**
     * @brief computes the type of variables in the relational expressions.
     * @details this also computes types of scalar expressions in the given expression graph only if they are needed
     *      to compute types of declared variables in the graph.
     * @param validate whether or not the operation validates the all sub-elements,
     *      if `false`, this computes the sub-elements only for decide the variable types
     * @param graph the target expression graph
     * @param repo the type repository
     * @return true if resolution was success
     * @return false if resolution was not success, please refer diagnostics()
     */
    bool resolve(
            ::takatori::relation::graph_type const& graph,
            bool validate = false,
            type::repository& repo = type::default_repository());

    /**
     * @brief computes type of variables in the step of execution plan.
     * @param step the target step
     * @param validate whether or not the operation validates the all sub-elements,
     *      if `false`, this computes the sub-elements only for decide the variable types
     * @param recursive if true, this also resolves the upstream steps.
     *      Or never resolves the upstream steps, that is, each variable which is referred from
     *      the expression must be resolved before this operation.
     * @param repo the type repository
     * @return true if resolution was success
     * @return false if resolution was not success, please refer diagnostics()
     */
    bool resolve(
            ::takatori::plan::step const& step,
            bool validate = false,
            bool recursive = false,
            type::repository& repo = type::default_repository());

    /**
     * @brief computes type of variables in the execution plan.
     * @param graph the target plan
     * @param validate whether or not the operation validates the all sub-elements,
     *      if `false`, this computes the sub-elements only for decide the variable types
     * @param repo the type repository
     * @return true if resolution was success
     * @return false if resolution was not success, please refer diagnostics()
     */
    bool resolve(
            ::takatori::plan::graph_type const& graph,
            bool validate = false,
            type::repository& repo = type::default_repository());

    /**
     * @brief computes type of variables in the statement.
     * @param statement the target statement
     * @param validate whether or not the operation validates the all sub-elements,
     *      if `false`, this computes the sub-elements only for decide the variable types
     * @param repo the type repository
     * @return true if resolution was success
     * @return false if resolution was not success, please refer diagnostics()
     */
    bool resolve(
            ::takatori::statement::statement const& statement,
            bool validate = false,
            type::repository& repo = type::default_repository());

    /**
     * @brief returns whether or not this saw any diagnostics while analyzing scalar expressions.
     * @return true if there are any diagnostics
     * @return false otherwise
     */
    [[nodiscard]] bool has_diagnostics() const noexcept;

    /**
     * @brief returns diagnostics while analyzing scalar expressions.
     * @return the diagnostics
     */
    [[nodiscard]] ::takatori::util::sequence_view<diagnostic_type const> diagnostics() const noexcept;

    /**
     * @brief remove all diagnostics while analyzing scalar expression.
     */
    void clear_diagnostics() noexcept;

private:
    ::takatori::util::maybe_shared_ptr<expression_mapping> expressions_;
    ::takatori::util::maybe_shared_ptr<variable_mapping> variables_;
    bool allow_unresolved_ { true };
    std::vector<diagnostic_type> diagnostics_;
};

} // namespace yugawara::analyzer
