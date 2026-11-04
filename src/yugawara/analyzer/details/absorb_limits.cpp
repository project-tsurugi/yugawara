#include "absorb_limits.h"

#include <optional>

#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>

#include "ordered_relation.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

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
    // FIXME: currently we do not absorb the limit with the variable count (e.g. placeholders)
    if (auto count = slice.count().immediate_if()) {
        return *count;
    }
    return {};
}

class engine {
public:
    explicit engine(runtime_feature_set const& features) noexcept :
        features_ { features }
    {}

    void process(relation::graph_type& graph) {
        for (auto&& expr : graph) {
            if (expr.kind() == relation::intermediate::limit::tag) {
                absorb(unsafe_downcast<relation::intermediate::limit>(expr));
            }
        }
    }

private:
    runtime_feature_set const& features_;

    [[nodiscard]] bool can_absorb_count(relation::scan const& scan, path_capability_set const& capabilities) const {
        // NOTE: each row of the scan must become exactly one input row of the limit
        return features_.contains(runtime_feature::scan_limit)
                && capabilities.contains(path_capability::at_least_one)
                && capabilities.contains(path_capability::at_most_one)
                && !scan.row_slice();
    }

    void absorb(relation::intermediate::limit& expr) {
        auto count = find_target_count(expr);
        if (!count) {
            return;
        }
        auto path = find_ordered_source(expr);
        if (!path.end || path.end->kind() != relation::scan::tag) {
            return;
        }
        auto&& scan = unsafe_downcast<relation::scan>(*path.end);
        if (!can_absorb_count(scan, path.capabilities)) {
            return;
        }
        // NOTE: the scan must already provide the row order of the limit (see rewrite_scan()),
        // because taking the row count before sorting the rows changes the result
        if (!expr.sort_keys().empty()) {
            auto direction = find_scan_direction(scan, expr);
            if (!direction || scan.direction() != direction) {
                return;
            }
        }
        scan.row_slice() = relation::scan::row_slice_type { {}, *count };
    }
};

} // namespace

void absorb_limits(
        ::takatori::relation::graph_type& graph,
        runtime_feature_set const& features) {
    engine e { features };
    e.process(graph);
}

} // namespace yugawara::analyzer::details
