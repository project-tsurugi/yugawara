#pragma once

#include <takatori/relation/expression.h>

#include <takatori/plan/step.h>

#include <takatori/graph/graph.h>

#include "join_info.h"
#include "details/step_plan_builder_options.h"

namespace yugawara::analyzer {

/**
 * @brief builds a step plan from intermediate plan.
 * @details this also replaces intermediate representation of join (a.k.a. `join_relation`) operations,
 *      according to the predefined join hints.
 */
class step_plan_builder {
public:
    /// @brief options type.
    using options_type = details::step_plan_builder_options;

    /**
     * @brief creates a new instance.
     */
    step_plan_builder() = default;

    /**
     * @brief creates a new instance.
     * @param options planning options
     */
    explicit step_plan_builder(options_type options) noexcept;

    /**
     * @brief returns the underlying planning options.
     * @return the underlying planning options
     */
    [[nodiscard]] options_type& options() noexcept;

    /// @copydoc options()
    [[nodiscard]] options_type const& options() const noexcept;

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
    [[nodiscard]] ::takatori::graph::graph<::takatori::plan::step> operator()(
            ::takatori::graph::graph<::takatori::relation::expression>&& graph) const;

private:
    options_type options_;
};

} // namespace yugawara::analyzer
