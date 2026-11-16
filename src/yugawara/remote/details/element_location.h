#pragma once

#include <takatori/scalar/expression.h>
#include <takatori/relation/expression.h>
#include <takatori/plan/step.h>
#include <takatori/plan/process.h>
#include <takatori/plan/exchange.h>
#include <takatori/statement/statement.h>

#include <yugawara/remote/location_options.h>
#include <yugawara/remote/symbol_location_query.h>

namespace yugawara::remote::details {

/**
 * @brief computes the locations where each element of compiled statements can be executed.
 * @details The location of each element (operator, step, or statement) is decided as follows:
 *
 *      - elements which may refer to tables: the intersection of the locations of the referred relations,
 *        where the location of each table index is the location of its table,
 *        and the location of the other relations (e.g. exchanges) is only the local location
 *      - elements which never restrict the location (`offer`, `take_flat`, `take_group`, and `take_cogroup` operators):
 *        all available locations
 *      - `filter` operators: the locations where their conditions can be evaluated
 *      - process: the intersection of the locations of its operators
 *      - `execute` statement: the intersection of the locations of its steps
 *      - other elements: the local location only
 *
 *      The locations of `scan` and `find` operators are also restricted to the locations where
 *      their search keys can be evaluated, and the locations of the variables in the row slice of `scan`.
 *
 *      Additionally, the locations of each operator are restricted to the local location
 *      if its kind is not in remote::location_options::remote_relation_expression_kinds().
 *
 *      Note that this inspects scalar expressions only in the elements described above.
 */
class element_location {
public:
    /// @brief set of numeric location IDs type.
    using location_id_set_type = remote::location_info::location_id_set_type;

    /**
     * @brief creates a new instance.
     * @details This keeps a copy of the given options, but the symbol location query in them is shared.
     * @param options the location options, which must have a symbol location query
     */
    explicit element_location(remote::location_options options) noexcept;

    /**
     * @brief returns all available locations.
     * @return all available locations
     */
    [[nodiscard]] location_id_set_type all() const;

    /**
     * @brief returns the locations where the given scalar expression can be evaluated.
     * @details The result is the intersection of the locations of all nodes in the expression tree:
     *
     *      - nodes whose kind is not in remote::location_options::remote_scalar_expression_kinds():
     *        the local location only
     *      - function calls: the locations of the function
     *      - references to external variables (e.g. host variables): the locations of the variable
     *      - extensions: the local location only, because this does not inspect their contents
     *      - other nodes: all available locations
     *
     *      If the referred function is not a function declaration, the function call is restricted to
     *      the local location.
     * @param expression the target expression
     * @return the locations
     */
    [[nodiscard]] location_id_set_type operator()(::takatori::scalar::expression const& expression) const;

    /**
     * @brief returns the locations where the given operator can be executed.
     * @param expression the target operator
     * @return the locations
     */
    [[nodiscard]] location_id_set_type operator()(::takatori::relation::expression const& expression) const;

    /**
     * @brief returns the locations where the given step (process or exchange) can be executed.
     * @param step the target step
     * @return the locations
     */
    [[nodiscard]] location_id_set_type operator()(::takatori::plan::step const& step) const;

    /**
     * @brief returns the locations where the given process can be executed.
     * @param step the target process
     * @return the locations
     */
    [[nodiscard]] location_id_set_type operator()(::takatori::plan::process const& step) const;

    /**
     * @brief returns the locations where the given exchange can be executed.
     * @param step the target exchange
     * @return the locations
     */
    [[nodiscard]] location_id_set_type operator()(::takatori::plan::exchange const& step) const;

    /**
     * @brief returns the locations where the given statement can be executed.
     * @param statement the target statement
     * @return the locations
     */
    [[nodiscard]] location_id_set_type operator()(::takatori::statement::statement const& statement) const;

private:
    remote::location_options options_;
    remote::symbol_location_query const& query_;
};

} // namespace yugawara::remote::details
