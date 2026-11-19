#pragma once

#include <vector>

#include <takatori/relation/graph.h>

#include <yugawara/diagnostic.h>
#include <yugawara/remote/location_options.h>

#include "intermediate_plan_normalizer_code.h"

namespace yugawara::analyzer {

/**
 * @brief normalizer for intermediate execution plan.
 */
class intermediate_plan_normalizer {
public:
    /// @brief the diagnostic type of this normalizer.
    using diagnostic_type = diagnostic<intermediate_plan_normalizer_code>;

    /**
     * @brief creates a new instance.
     */
    intermediate_plan_normalizer() = default;

    /**
     * @brief returns the location options.
     * @details If the options has a symbol location query, this normalizer inserts fences
     *      into the boundaries of the execution locations.
     * @return the location options
     */
    // NOTE: if this normalizer has more options, we put them into a class like intermediate_plan_optimizer_options
    [[nodiscard]] remote::location_options& location_options() noexcept;

    /// @copydoc location_options()
    [[nodiscard]] remote::location_options const& location_options() const noexcept;

    /**
     * @brief normalizes the given intermediate execution plan.
     * @param graph the target intermediate execution plan
     * @return empty if the plan was successfully normalized
     * @return diagnostics with intermediate_plan_normalizer_code::empty_operation_location
     *      if there are operators which cannot be executed in any locations,
     *      unless location_options().allow_empty_operation_location() is enabled
     * @return other diagnostics if the plan cannot be normalized
     */
    [[nodiscard]] std::vector<diagnostic_type> operator()(::takatori::relation::graph_type& graph);

private:
    remote::location_options location_options_ {};
};

} // namespace yugawara::analyzer
