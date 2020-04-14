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
     * @param creator the object creator
     */
    explicit intermediate_plan_optimizer(::takatori::util::object_creator creator) noexcept;

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
     */
    void operator()(::takatori::relation::graph_type& graph);

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept {
        return options_.get_object_creator();
    }

private:
    options_type options_;
};

} // namespace yugawara::analyzer
