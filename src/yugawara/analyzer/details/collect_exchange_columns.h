#pragma once

#include <takatori/plan/graph.h>

#include "exchange_column_info_map.h"

namespace yugawara::analyzer::details {

/**
 * @brief fixes exchange columns of incomplete step plan.
 * @details This will fill the exchange columns to each exchange step and the corresponding I/O operations:
 *
 *      - `take_{flat, group, cogroup}`
 *      - `offer`
 *      - `join_{find, scan}` with broadcast
 *
 *      This requires stream variable definitions about the following operators:
 *
 *      `find.columns`
 *      `scan.columns`
 *      `join_find.columns` (only if `.source` points a index)
 *      `join_scan.columns` (only if `.source` points a index)
 *
 *      This must consider the following optional columns:
 *
 *      `offer.columns` (`.destination` can be a stream variable)
 *      stream variables in exchange operations
 *
 *      This generates still incomplete step plan which includes duplicated stream variables over the steps.
 *      The target step graph can include `escape` operators, then this operation will remain them
 *      (the succeeding stream_variable_rewriter will remove).
 *
 * @param graph the target incomplete step plan
 * @return the exchange column information for the individual exchange operations
 */
exchange_column_info_map collect_exchange_columns(::takatori::plan::graph_type& graph);

} // namespace yugawara::analyzer::details
