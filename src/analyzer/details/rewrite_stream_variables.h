#pragma once

#include <takatori/plan/graph.h>
#include <takatori/util/object_creator.h>

#include "exchange_column_info_map.h"

namespace yugawara::analyzer::details {

/**
 * @brief rewrites all stream variables in the given step plan.
 * @details This also removes unused stream variables and exchange columns.
 *      This accepts `escape` operators in process steps, but they will be removed in this operation.
 * @param exchange_map
 * @param graph
 * @param creator
 * @pre exchange_column_collector
 */
void rewrite_stream_variables(
        exchange_column_info_map& exchange_map,
        ::takatori::plan::graph_type& graph,
        ::takatori::util::object_creator creator);

} // namespace yugawara::analyzer::details
