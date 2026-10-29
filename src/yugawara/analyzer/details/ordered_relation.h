#pragma once

#include <optional>
#include <vector>

#include <takatori/relation/expression.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/scan_direction.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/storage/details/index_key_element.h>

namespace yugawara::analyzer::details {

/**
 * @brief returns the upstream `scan` operator, which can be the source of the row order of the given `limit`.
 * @details This walks from the `limit` to upstream only through the operators which preserve the row order
 *      (`filter` and `project`), and returns the `scan` operator if it reaches.
 *      This never considers whether or not the `scan` actually provides the rows in the order of the `limit`.
 * @param expr the target `limit` operator
 * @return the source `scan` operator
 * @return empty if there is no such the `scan` operator
 * @see find_scan_direction(::takatori::relation::scan const&, ::takatori::relation::intermediate::limit const&)
 */
[[nodiscard]] ::takatori::util::optional_ptr<::takatori::relation::scan> find_ordered_source(
        ::takatori::relation::intermediate::limit& expr);

/**
 * @brief returns the downstream operator, whose behavior may depend on the row order of the given `limit`.
 * @details This walks from the `limit` to downstream only through the operators which preserve the row order
 *      (`filter` and `project`), and returns the first operator which does not preserve the row order.
 *      This never considers whether or not the operator actually depends on the row order,
 *      for example, the `emit` operator depends on it only if it requires to preserve the input order.
 * @param expr the target `limit` operator
 * @return the destination operator
 * @return empty if there is no such the operator
 * @see find_ordered_source()
 */
[[nodiscard]] ::takatori::util::optional_ptr<::takatori::relation::expression> find_ordered_destination(
        ::takatori::relation::intermediate::limit& expr);

/**
 * @brief returns the sort keys of the given `limit`, as the columns of the given `scan`.
 * @param scan the source `scan` operator
 * @param expr the `limit` operator which requests the row order
 * @return the sort keys as the columns of the `scan`
 * @return empty if any sort keys do not refer the columns of the `scan`
 * @see find_ordered_source()
 */
[[nodiscard]] std::optional<std::vector<storage::details::index_key_element>> resolve_sort_keys(
        ::takatori::relation::scan const& scan,
        ::takatori::relation::intermediate::limit const& expr);

/**
 * @brief returns the direction to read the index of the given `scan`, which provides the rows in the order of the
 *      sort keys of the given `limit`.
 * @details The sort keys must refer the columns of the `scan`, and the leading keys of the `scan` range
 *      which have the same value on both ends are treated as fixed columns.
 *      This never considers the current direction of the `scan`.
 * @param scan the source `scan` operator
 * @param expr the `limit` operator which requests the row order
 * @return the direction to read the index, which provides the rows in the order of the sort keys
 * @return empty if the `scan` cannot provide the rows in the order
 * @see find_ordered_source()
 * @see find_scan_direction(storage::index const&, ::takatori::util::sequence_view<storage::details::index_key_element const>, ::takatori::util::sequence_view<std::reference_wrapper<storage::column const> const>)
 */
[[nodiscard]] std::optional<::takatori::relation::scan_direction> find_scan_direction(
        ::takatori::relation::scan const& scan,
        ::takatori::relation::intermediate::limit const& expr);

} // namespace yugawara::analyzer::details
