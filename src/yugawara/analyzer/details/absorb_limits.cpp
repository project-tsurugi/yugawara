#include "absorb_limits.h"

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

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

std::optional<std::size_t> find_target_count(relation::intermediate::limit const& expr) {
    if (!expr.group_keys().empty()) {
        return {};
    }
    auto&& slice = expr.row_slice();
    // FIXME: currently we do not absorb the limit with the effective start position (other than immediate `0`)
    if (auto&& start = slice.start(); !start.is_absent()) {
        if (auto position = start.immediate_if(); !position || *position != 0) {
            return {};
        }
    }
    if (auto count = slice.count().immediate_if()) {
        return *count;
    }
    return {};
}

optional_ptr<relation::scan> find_target_scan(relation::intermediate::limit& expr) {
    auto upstream = expr.input().opposite();
    if (!upstream) {
        return {};
    }
    auto&& owner = upstream->owner();
    if (owner.kind() != relation::scan::tag) {
        return {};
    }
    auto&& scan = unsafe_downcast<relation::scan>(owner);
    if (scan.row_slice()) {
        return {};
    }
    return scan;
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

std::optional<relation::scan_direction> find_direction(
        relation::scan const& scan,
        relation::intermediate::limit const& expr) {
    std::vector<storage::details::index_key_element> sort_keys {};
    sort_keys.reserve(expr.sort_keys().size());
    for (auto&& key : expr.sort_keys()) {
        auto column = find_source_column(scan, key.variable());
        if (!column) {
            return {};
        }
        sort_keys.emplace_back(*column, key.direction());
    }
    auto&& index = binding::extract<storage::index>(scan.source());
    auto fixed_columns = collect_fixed_columns(scan);
    return find_scan_direction(index, sort_keys, fixed_columns);
}

void process(relation::intermediate::limit& expr) {
    auto count = find_target_count(expr);
    if (!count) {
        return;
    }
    auto scan = find_target_scan(expr);
    if (!scan) {
        return;
    }
    if (!expr.sort_keys().empty()) {
        auto direction = find_direction(*scan, expr);
        if (!direction) {
            return;
        }
        // adopt the direction only if the scan does not specify any directions, or it is same as the required one
        if (auto&& current = scan->direction(); current && *current != *direction) {
            return;
        }
        scan->direction() = direction;
    }
    scan->row_slice() = relation::scan::row_slice_type { {}, *count };
}

} // namespace

void absorb_limits(::takatori::relation::graph_type& graph) {
    for (auto&& expr : graph) {
        if (expr.kind() == relation::intermediate::limit::tag) {
            process(unsafe_downcast<relation::intermediate::limit>(expr));
        }
    }
}

} // namespace yugawara::analyzer::details
