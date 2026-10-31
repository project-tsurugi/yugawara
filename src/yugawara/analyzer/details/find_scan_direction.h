#pragma once

#include <functional>
#include <optional>

#include <takatori/relation/scan_direction.h>

#include <takatori/util/sequence_view.h>

#include <yugawara/storage/column.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/details/index_key_element.h>

namespace yugawara::analyzer::details {

/**
 * @brief returns the direction to read the given index, which provides the rows in the requested order.
 * @details The index provides the rows in the order of its keys, or in the reversed order if it is read backward.
 *      The requested order is satisfied if the sort keys, except the fixed columns,
 *      match a prefix of the index keys, except the fixed columns, and if their directions are
 *      all the same as the index keys (forward), or all reversed from them (backward).
 *      If the index is a primary index and all its keys are matched or fixed,
 *      the rest of the sort keys are also satisfied, because the primary key is unique and never null.
 * @note This never assumes that the primary keys follow the keys of a secondary index,
 *      and never applies the above uniqueness rule to the other unique indices, because they may contain nulls.
 * @note If both directions satisfy the requested order (e.g. all sort keys are fixed), this returns the forward one.
 * @note This never considers whether or not the backward scan is available, so that the caller must check it.
 * @param index the target index
 * @param sort_keys the requested order, as a sequence of the index columns and their directions
 * @param fixed_columns the index columns that have a single value in the scan range,
 *      for example, the columns in the equivalent conditions of the range.
 *      Note that, the values of the columns must not change while the scan.
 * @return the direction to read the index, which provides the rows in the requested order
 * @return empty if the index cannot provide the rows in the requested order
 */
[[nodiscard]] std::optional<::takatori::relation::scan_direction> find_scan_direction(
        storage::index const& index,
        ::takatori::util::sequence_view<storage::details::index_key_element const> sort_keys,
        ::takatori::util::sequence_view<std::reference_wrapper<storage::column const> const> fixed_columns);

} // namespace yugawara::analyzer::details
