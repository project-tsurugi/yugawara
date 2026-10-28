#pragma once

#include <functional>
#include <vector>

#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/storage/index.h>

#include "scan_key_collector.h"

namespace yugawara::analyzer::details {

/**
 * @brief an index candidate for a scan operation.
 * @details The terms point to the elements in the scan_key_collector which built this candidate,
 *      so that this must not outlive it.
 *
 *      Applying the candidate to the operator graph purges the terms from their original conditions,
 *      and replaces them with `TRUE` (see search_key_term::purge_*()).
 *      The rest conditions and the orphaned operators must be handled by the caller.
 */
struct index_candidate {
    /// @brief the target index.
    std::reference_wrapper<storage::index const> index;

    /// @brief the estimated result.
    index_estimator::result result;

    /// @brief the terms of the search key, in order of the index keys.
    std::vector<scan_key_collector::term*> terms;
};

/**
 * @brief estimates the index access with the collected scan key terms.
 * @details This never modifies the operator graph nor the terms,
 *      so that the caller can compare candidates of individual indices before applying one of them.
 *
 *      The resulting candidate may have no terms if the index cannot use any of them.
 * @param collector the collector of the target scan
 * @param index the target index
 * @param estimator the index cost estimator
 * @return the estimated candidate
 */
[[nodiscard]] index_candidate estimate_index(
        scan_key_collector& collector,
        storage::index const& index,
        index_estimator const& estimator);

} // namespace yugawara::analyzer::details
