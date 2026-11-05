#include "absorb_limits.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include "path_capability.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

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
 * @brief returns the upstream `scan` operators whose each row becomes one or more input rows of the given `limit`.
 * @details This walks the upstream operators only while the path from the input to the output has
 *      `at_least_one` (see find_path_capabilities()).
 *      If the operator has two or more inputs, this walks each input which has `at_least_one`
 *      (e.g. the left input of left outer joins, or each input of union all).
 * @param expr the target limit
 * @return the upstream scans, may be empty
 */
std::vector<relation::scan*> find_source_scans(relation::intermediate::limit& expr) {
    std::vector<relation::scan*> results {};
    std::vector<relation::expression::output_port_type*> next {};
    if (auto upstream = expr.input().opposite()) {
        next.emplace_back(upstream.get());
    }
    while (!next.empty()) {
        auto* upstream = next.back();
        next.pop_back();
        auto&& owner = upstream->owner();
        if (owner.kind() == relation::scan::tag) {
            results.emplace_back(std::addressof(unsafe_downcast<relation::scan>(owner)));
            continue;
        }
        for (auto&& input : owner.input_ports()) {
            auto capabilities = find_path_capabilities(input, *upstream);
            if (!capabilities.contains(path_capability::at_least_one)) {
                continue;
            }
            if (auto opposite = input.opposite()) {
                next.emplace_back(opposite.get());
            }
        }
    }
    return results;
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
        for (auto* scan : find_source_scans(expr)) {
            absorb(*scan, *count);
        }
    }

    static void absorb(relation::scan& scan, std::size_t count) {
        auto&& slice = scan.row_slice();
        // NOTE: the scan without the row count is regarded as the one with the infinite row count
        if (auto&& current = slice.count(); !current.is_absent()) {
            // FIXME: currently we do not compare with the variable count of the scan
            auto value = current.immediate_if();
            if (!value) {
                return;
            }
            count = std::min(count, *value);
        }
        scan.row_slice() = relation::scan::row_slice_type { slice.start(), count };
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
