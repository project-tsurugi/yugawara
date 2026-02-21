#pragma once

#include <takatori/relation/graph.h>

#include "details/intermediate_plan_optimizer_options.h"

namespace yugawara::analyzer {

/**
 * @brief optimizer for intermediate execution plan.
 */
class intermediate_plan_optimizer {
public:
    /// @brief the optimizer options type.
    using options_type = details::intermediate_plan_optimizer_options;

    /**
     * @brief creates a new instance.
     */
    intermediate_plan_optimizer() = default;

    /**
     * @brief creates a new instance.
     * @param options the optimizer options
     */
    explicit intermediate_plan_optimizer(options_type options) noexcept;

    /**
     * @brief returns the optimizer options.
     * @return the optimizer options
     */
    options_type& options() noexcept;

    /// @brief options()
    [[nodiscard]] options_type const& options() const noexcept;

    /**
     * @brief optimizes the given intermediate execution plan.
     * @param graph the target intermediate execution plan
     * @attention the input graph must be a normalized intermediate execution plan.
     *      Apply intermediate_plan_normalizer before execute this function.
     * @see intermediate_plan_normalizer
     */
    void operator()(::takatori::relation::graph_type& graph);

private:
    options_type options_;
};

} // namespace yugawara::analyzer
