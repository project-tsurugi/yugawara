#pragma once

#include <takatori/relation/expression.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/plan/step.h>

#include <takatori/graph/graph.h>

#include "join_info.h"
#include "details/step_planning_info.h"

namespace yugawara::analyzer {

/**
 * @brief builds a step plan from intermediate plan.
 * @details this also replaces intermediate representation of join (a.k.a. `join_relation`) operations,
 *      according to the predefined join hints.
 */
class step_plan_builder {
public:
    /// @brief planning information type.
    using planning_info = details::step_planning_info;

    /**
     * @brief creates a new instance.
     */
    step_plan_builder() = default;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit step_plan_builder(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief creates a new instance.
     * @param info planning information
     */
    explicit step_plan_builder(planning_info info) noexcept;

    /**
     * @brief returns the underlying planning information.
     * @return the underlying planning information
     */
    planning_info& info() noexcept;

    /// @copydoc info()
    planning_info const& info() const noexcept;

    /**
     * @brief registers hint for the given join operation.
     * @param expr the join operation
     * @param info the hint information
     * @return this
     * @throws std::invalid_argument if the target join expression has been already registered
     */
    step_plan_builder& add(::takatori::relation::intermediate::join const& expr, join_info info);

    /**
     * @brief builds a step plan from the given intermediate plan.
     * @details This rewrites intermediate plan operators by only simple rules.
     *      To obtain more optimized plan, please rewrite plan before/after this operation directly.
     *      For example, this always convert `intersection_relation (with distinct)` into `intersection_group`,
     *      but it can alternatively achieve to use semi-join operation.
     * @param graph the intermediate plan
     * @return the built step plan
     * @attention variable descriptors in the given plan will be replaced other ones
     */
    ::takatori::graph::graph<::takatori::plan::step> build(
            ::takatori::graph::graph<::takatori::relation::expression>&& graph) noexcept;

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    ::takatori::util::object_creator get_object_creator() const noexcept;

private:
    planning_info info_;
};

} // namespace yugawara::analyzer
