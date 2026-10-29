#include "absorb_limits.h"

#include <optional>

#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include "ordered_relation.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

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
        auto direction = find_scan_direction(*scan, expr);
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
