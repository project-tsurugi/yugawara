#pragma once

#include <takatori/relation/expression.h>
#include <takatori/plan/step.h>
#include <takatori/plan/process.h>
#include <takatori/plan/exchange.h>
#include <takatori/statement/statement.h>

#include <yugawara/remote/symbol_location_query.h>

namespace yugawara::details {

/**
 * @brief computes the locations where each element of compiled statements can be executed.
 * @details The location of each element (operator, step, or statement) is decided as follows:
 *
 *      - elements which may refer to tables: the intersection of the locations of the referred relations,
 *        where the location of each table index is the location of its table,
 *        and the location of the other relations (e.g. exchanges) is only the local location
 *      - elements which never restrict the location (`offer`, `take_flat`, `take_group`, and `take_cogroup` operators):
 *        all available locations
 *      - process: the intersection of the locations of its operators
 *      - `execute` statement: the intersection of the locations of its steps
 *      - other elements: the local location only
 *
 *      Note that this does not consider symbols in scalar expressions.
 */
class element_location {
public:
    /// @brief set of numeric location IDs type.
    using location_id_set_type = remote::location_info::location_id_set_type;

    /**
     * @brief creates a new instance.
     * @param query the symbol location query
     */
    explicit element_location(remote::symbol_location_query const& query) noexcept;

    /**
     * @brief returns all available locations.
     * @return all available locations
     */
    [[nodiscard]] location_id_set_type all() const;

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
    remote::symbol_location_query const& query_;
};

} // namespace yugawara::details
