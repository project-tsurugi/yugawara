#pragma once

#include <optional>

#include <takatori/relation/details/row_slice.h>

namespace yugawara::analyzer::details {

/**
 * @brief returns whether or not the given row slice has the effective start position.
 * @details The start position is effective unless it is absent or the immediate `0`.
 * @param slice the target row slice
 * @return true if the row slice has the effective start position
 * @return false otherwise
 */
[[nodiscard]] bool has_start(::takatori::relation::details::row_slice const& slice);

/**
 * @brief returns the row slice which has the smaller row count of the given ones.
 * @details The row slice without the row count is regarded as the one with the infinite row count.
 *      The row counts can be merged only if either of them is absent, they are the same (including the same
 *      variables), or both are immediate values.
 * @param first the first row slice
 * @param second the second row slice
 * @return the merged row slice
 * @return empty if they cannot be merged, including the case that either of them has the effective start position
 * @see has_start()
 */
[[nodiscard]] std::optional<::takatori::relation::details::row_slice> merge_row_slice(
        ::takatori::relation::details::row_slice const& first,
        ::takatori::relation::details::row_slice const& second);

} // namespace yugawara::analyzer::details
