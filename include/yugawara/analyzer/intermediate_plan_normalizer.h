#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/diagnostic.h>

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
     * @brief normalizes the given intermediate execution plan.
     * @param graph the target intermediate execution plan
     */
    void operator()(::takatori::relation::graph_type& graph);
};

} // namespace yugawara::analyzer
