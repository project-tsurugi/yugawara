#pragma once

#include <takatori/relation/graph.h>
#include <takatori/plan/graph.h>
#include <takatori/util/object_creator.h>

namespace yugawara::analyzer::details {

/**
 * @brief collects individual process operator graphs.
 * @details The source operator graph must be applied collect_exchange_steps(),
 *      and include step plan operators in the source immediate plan.
 *
 *      This creates process steps from the individual process operators,
 *      but does not compute successors or predecessors of generated.
 *      To complete neighborhood information of individual steps, please call collect_step_relations().
 *
 *      Finally, the all operators in the source plan will be migrated into the destination step plan as
 *      individual process steps.
 * @param source the source intermediate plan
 * @param destination the destination incomplete step plan
 * @param creator the object creator for the working area
 */
void collect_process_steps(
        ::takatori::relation::graph_type&& source,
        ::takatori::plan::graph_type& destination,
        ::takatori::util::object_creator creator);

} // namespace yugawara::analyzer::details
