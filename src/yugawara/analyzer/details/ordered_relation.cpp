#include "ordered_relation.h"

#include <algorithm>
#include <functional>
#include <vector>

#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>

#include <takatori/util/downcast.h>

#include <yugawara/binding/extract.h>

#include <yugawara/storage/column.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/details/index_key_element.h>

#include "find_scan_direction.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

using column_ref = std::reference_wrapper<storage::column const>;

namespace {

// NOTE: each order preserving operator must have exactly one input and one output,
// and must not increase the number of rows
bool is_order_preserving(relation::expression const& expr) noexcept {
    switch (expr.kind()) {
        case relation::filter::tag:
        case relation::project::tag:
            return true;
        default:
            return false;
    }
}

optional_ptr<relation::expression> find_upstream(relation::expression::input_port_type& port) {
    if (auto upstream = port.opposite()) {
        return upstream->owner();
    }
    return {};
}

optional_ptr<relation::expression> find_downstream(relation::expression::output_port_type& port) {
    if (auto downstream = port.opposite()) {
        return downstream->owner();
    }
    return {};
}

optional_ptr<storage::column const> find_source_column(relation::scan const& scan, descriptor::variable const& variable) {
    for (auto&& column : scan.columns()) {
        if (column.destination() == variable) {
            return binding::extract_if<storage::column>(column.source());
        }
    }
    return {};
}

std::vector<column_ref> collect_fixed_columns(relation::scan const& scan) {
    std::vector<column_ref> results {};
    auto&& lower = scan.lower().keys();
    auto&& upper = scan.upper().keys();
    for (std::size_t i = 0, n = std::min(lower.size(), upper.size()); i < n; ++i) {
        auto&& l = lower[i];
        auto&& u = upper[i];
        // NOTE: the leading keys which have the same value on both ends have the single value in the range
        if (l.variable() != u.variable() || l.value() != u.value()) {
            break;
        }
        auto column = binding::extract_if<storage::column>(l.variable());
        if (!column) {
            break;
        }
        results.emplace_back(*column);
    }
    return results;
}

} // namespace

optional_ptr<relation::scan> find_ordered_source(relation::intermediate::limit& expr) {
    auto current = find_upstream(expr.input());
    while (current && is_order_preserving(*current)) {
        current = find_upstream(current->input_ports()[0]);
    }
    if (current && current->kind() == relation::scan::tag) {
        return unsafe_downcast<relation::scan>(*current);
    }
    return {};
}

namespace {

optional_ptr<relation::expression> find_ordered_destination(relation::expression::output_port_type& output) {
    auto current = find_downstream(output);
    while (current && is_order_preserving(*current)) {
        current = find_downstream(current->output_ports()[0]);
    }
    return current;
}

} // namespace

optional_ptr<relation::expression> find_ordered_destination(relation::intermediate::limit& expr) {
    return find_ordered_destination(expr.output());
}

optional_ptr<relation::expression> find_ordered_destination(relation::scan& expr) {
    return find_ordered_destination(expr.output());
}

std::optional<std::vector<storage::details::index_key_element>> resolve_sort_keys(
        relation::scan const& scan,
        relation::intermediate::limit const& expr) {
    std::vector<storage::details::index_key_element> results {};
    results.reserve(expr.sort_keys().size());
    for (auto&& key : expr.sort_keys()) {
        auto column = find_source_column(scan, key.variable());
        if (!column) {
            return {};
        }
        results.emplace_back(*column, key.direction());
    }
    return results;
}

std::optional<relation::scan_direction> find_scan_direction(
        relation::scan const& scan,
        relation::intermediate::limit const& expr) {
    auto sort_keys = resolve_sort_keys(scan, expr);
    if (!sort_keys) {
        return {};
    }
    auto&& index = binding::extract<storage::index>(scan.source());
    auto fixed_columns = collect_fixed_columns(scan);
    return find_scan_direction(index, *sort_keys, fixed_columns);
}

} // namespace yugawara::analyzer::details
