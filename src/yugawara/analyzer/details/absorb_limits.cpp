#include "absorb_limits.h"

#include <algorithm>
#include <optional>

#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include "path_capability.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

namespace {

std::optional<std::size_t> find_target_count(relation::intermediate::limit const& expr) {
    // NOTE: the limits with sort keys are handled by rewrite_scan()
    if (!expr.group_keys().empty() || !expr.sort_keys().empty()) {
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

/**
 * @brief returns the upstream `scan` whose each row becomes exactly one input row of the given `limit`.
 * @details This walks the upstream operators which have a single input, only while the path from the input
 *      to the output has both `at_least_one` and `at_most_one` (see find_path_capabilities()).
 *      This never walks the operators which have two or more inputs (e.g. the left input of left outer joins),
 *      because push_down_limits() places the partial limits into such the inputs.
 * @param expr the target limit
 * @return the upstream scan
 * @return empty if there is no such the scan
 */
optional_ptr<relation::scan> find_source_scan(relation::intermediate::limit& expr) {
    auto upstream = expr.input().opposite();
    while (upstream) {
        auto&& owner = upstream->owner();
        if (owner.kind() == relation::scan::tag) {
            return unsafe_downcast<relation::scan>(owner);
        }
        auto&& inputs = owner.input_ports();
        if (inputs.size() != 1) {
            return {};
        }
        auto&& input = inputs[0];
        auto capabilities = find_path_capabilities(input, *upstream);
        if (!capabilities.contains(path_capability::at_least_one)
                || !capabilities.contains(path_capability::at_most_one)) {
            return {};
        }
        upstream = input.opposite();
    }
    return {};
}

class engine {
public:
    explicit engine(runtime_feature_set const& features) noexcept :
        features_ { features }
    {}

    void process(relation::graph_type& graph) {
        if (!features_.contains(runtime_feature::scan_limit)) {
            return;
        }
        for (auto&& expr : graph) {
            if (expr.kind() == relation::intermediate::limit::tag) {
                absorb(unsafe_downcast<relation::intermediate::limit>(expr));
            }
        }
    }

private:
    runtime_feature_set const& features_;

    static void absorb(relation::intermediate::limit& expr) {
        auto count = find_target_count(expr);
        if (!count) {
            return;
        }
        auto scan = find_source_scan(expr);
        if (!scan) {
            return;
        }
        auto&& slice = scan->row_slice();
        // NOTE: the scan without the row count is regarded as the one with the infinite row count
        if (auto&& current = slice.count(); !current.is_absent()) {
            // FIXME: currently we do not compare with the variable count of the scan
            auto value = current.immediate_if();
            if (!value) {
                return;
            }
            count = std::min(*count, *value);
        }
        scan->row_slice() = relation::scan::row_slice_type { slice.start(), *count };
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
