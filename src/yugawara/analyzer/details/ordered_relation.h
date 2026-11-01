#pragma once

#include <optional>
#include <vector>

#include <takatori/relation/expression.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/scan_direction.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/storage/details/index_key_element.h>

#include "path_capability.h"

namespace yugawara::analyzer::details {

/**
 * @brief represents a path which preserves the row order, and the operator just after the path.
 * @see find_ordered_destination(::takatori::relation::expression::output_port_type&)
 */
struct ordered_path {
    /// @brief the first operator which does not preserve the row order, or empty if the path is not connected.
    ::takatori::util::optional_ptr<::takatori::relation::expression> destination {};

    /// @brief the intersection of the capabilities of the operators in the path (see find_path_capabilities()).
    path_capability_set capabilities {};
};

/**
 * @brief returns the upstream `scan` operator, which can be the source of the row order of the given `limit`.
 * @details This walks from the `limit` to upstream only through the operators which have exactly one input and
 *      one output, and whose path preserves the row order (see find_path_capabilities()),
 *      and returns the `scan` operator if it reaches.
 *      This never considers whether or not the `scan` actually provides the rows in the order of the `limit`.
 * @param expr the target `limit` operator
 * @return the source `scan` operator
 * @return empty if there is no such the `scan` operator
 * @see find_scan_direction(::takatori::relation::scan const&, ::takatori::relation::intermediate::limit const&)
 */
[[nodiscard]] ::takatori::util::optional_ptr<::takatori::relation::scan> find_ordered_source(
        ::takatori::relation::intermediate::limit& expr);

/**
 * @brief returns the path from the given output port to downstream, which preserves the row order.
 * @details This walks from the output port to downstream only through the operators which have exactly one input
 *      and one output, and whose path preserves the row order (see find_path_capabilities()),
 *      and returns the first operator which does not satisfy them, with the capabilities of the path.
 *      This never considers whether or not the destination operator actually depends on the row order,
 *      for example, the `emit` operator depends on it only if it requires to preserve the input order.
 *      If the path is empty, the capabilities contain all `path_capability` values.
 * @param output the output port where the path starts
 * @return the path, whose destination is empty if the path is not connected to any operators
 */
[[nodiscard]] ordered_path find_ordered_destination(::takatori::relation::expression::output_port_type& output);

/**
 * @brief returns the path from the given `limit` to downstream, which preserves the row order.
 * @param expr the target `limit` operator
 * @return the path from the output of the `limit`
 * @see find_ordered_destination(::takatori::relation::expression::output_port_type&)
 */
[[nodiscard]] ordered_path find_ordered_destination(::takatori::relation::intermediate::limit& expr);

/**
 * @brief returns the path from the given `scan` to downstream, which preserves the row order.
 * @param expr the target `scan` operator
 * @return the path from the output of the `scan`
 * @see find_ordered_destination(::takatori::relation::expression::output_port_type&)
 */
[[nodiscard]] ordered_path find_ordered_destination(::takatori::relation::scan& expr);

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
