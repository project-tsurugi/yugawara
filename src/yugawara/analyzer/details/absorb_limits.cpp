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

bool is_adjacent(relation::scan const& scan, relation::intermediate::limit const& expr) {
    auto upstream = expr.input().opposite();
    return upstream && std::addressof(upstream->owner()) == std::addressof(scan);
}

/**
 * @brief returns whether or not the limit only requests the row order, without any row count nor group keys.
 * @param expr the target limit
 * @return true if it only requests the row order
 * @return false otherwise
 */
bool is_order_only(relation::intermediate::limit const& expr) {
    return expr.group_keys().empty()
            && !expr.sort_keys().empty()
            && !expr.row_slice();
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

    [[nodiscard]] bool can_absorb_count(relation::scan const& scan, relation::intermediate::limit const& expr) const {
        return features_.contains(runtime_feature::scan_limit)
                && is_adjacent(scan, expr)
                && !scan.row_slice();
    }

    [[nodiscard]] bool can_absorb_direction_only(relation::intermediate::limit const& expr) const {
        if (!features_.contains(runtime_feature::ordered_scan)) {
            return false;
        }
        // NOTE: the limit with row count becomes a forward exchange with ordering,
        // and the limit without row count will be removed (see remove_redundant_limits())
        return !expr.row_slice() || features_.contains(runtime_feature::ordered_forward);
    }

    void absorb(relation::intermediate::limit& expr) {
        auto count = find_target_count(expr);
        if (!count && !is_order_only(expr)) {
            return;
        }
        auto scan = find_ordered_source(expr);
        if (!scan) {
            return;
        }
        bool absorb_count = count && can_absorb_count(*scan, expr);
        if (!absorb_count && !can_absorb_direction_only(expr)) {
            return;
        }
        if (!expr.sort_keys().empty()) {
            auto direction = find_scan_direction(*scan, expr);
            if (!direction) {
                return;
            }
            if (*direction == relation::scan_direction::backward
                    && !features_.contains(runtime_feature::backward_scan)) {
                return;
            }
            // adopt the direction only if the scan does not specify any directions, or it is same as the required one
            if (auto&& current = scan->direction(); current && *current != *direction) {
                return;
            }
            scan->direction() = direction;
        }
        if (absorb_count) {
            scan->row_slice() = relation::scan::row_slice_type { {}, *count };
        }
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
