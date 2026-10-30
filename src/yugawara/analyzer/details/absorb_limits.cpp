#include "absorb_limits.h"

#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/extract.h>

#include <yugawara/storage/column.h>
#include <yugawara/storage/index.h>
#include <yugawara/storage/index_feature.h>
#include <yugawara/storage/provider.h>

#include "apply_index_to_scan.h"
#include "estimate_index.h"
#include "find_scan_direction.h"
#include "ordered_relation.h"
#include "remove_redundant_conditions.h"
#include "scan_key_collector.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::unsafe_downcast;

using attribute = details::index_estimator_result_attribute;
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

class engine {
public:
    explicit engine(index_estimator const& index_estimator, runtime_feature_set const& features) noexcept :
        index_estimator_ { index_estimator },
        features_ { features }
    {}

    void process(relation::graph_type& graph) {
        bool changed = false;
        for (auto&& expr : graph) {
            if (expr.kind() == relation::intermediate::limit::tag) {
                if (select_index(unsafe_downcast<relation::intermediate::limit>(expr))) {
                    changed = true;
                }
            }
        }
        if (changed) {
            relation::merge_into(std::move(work_graph_), graph);
            work_graph_.clear();
            // NOTE: the selected index may consume the filter conditions, and then the scan may become adjacent to the limit
            remove_redundant_conditions(graph);
        }
        for (auto&& expr : graph) {
            if (expr.kind() == relation::intermediate::limit::tag) {
                absorb(unsafe_downcast<relation::intermediate::limit>(expr));
            }
        }
    }

private:
    index_estimator const& index_estimator_;
    runtime_feature_set const& features_;
    relation::graph_type work_graph_ {};

    [[nodiscard]] bool can_absorb_count(relation::scan const& scan, relation::intermediate::limit const& expr) const {
        return features_.contains(runtime_feature::scan_limit)
                && is_adjacent(scan, expr)
                && !scan.row_slice();
    }

    [[nodiscard]] bool can_absorb_direction_only() const {
        return features_.contains(runtime_feature::ordered_scan)
                && features_.contains(runtime_feature::ordered_forward);
    }

    void absorb(relation::intermediate::limit& expr) {
        auto count = find_target_count(expr);
        if (!count) {
            return;
        }
        auto scan = find_ordered_source(expr);
        if (!scan) {
            return;
        }
        bool absorb_count = can_absorb_count(*scan, expr);
        if (!absorb_count && !can_absorb_direction_only()) {
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
        if (absorb_count) {
            scan->row_slice() = relation::scan::row_slice_type { {}, *count };
        }
    }

    bool select_index(relation::intermediate::limit& expr) {
        if (expr.sort_keys().empty() || !find_target_count(expr)) {
            return false;
        }
        auto scan = find_ordered_source(expr);
        if (!scan) {
            return false;
        }
        if (!can_absorb_count(*scan, expr) && !can_absorb_direction_only()) {
            return false;
        }
        // NOTE: the scan conditions or the other limits have already decided the index
        if (scan->lower() || scan->upper() || scan->row_slice() || scan->direction()) {
            return false;
        }
        if (find_scan_direction(*scan, expr)) {
            // the current index already provides the row order
            return false;
        }
        auto sort_keys = resolve_sort_keys(*scan, expr);
        if (!sort_keys) {
            return false;
        }
        auto&& table = binding::extract<storage::index>(scan->source()).table();
        auto storages = table.owner();
        if (!storages) {
            // there is no index information
            return false;
        }

        // NOTE: the collector and the candidate are only valid while processing the current scan
        scan_key_collector collector {};
        bool has_terms = collector(*scan, scan_key_collector::scope::scan);
        std::optional<index_candidate> saved {};
        storages->each_table_index(
                table,
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    if (!entry->features().contains(storage::index_feature::scan)) {
                        return;
                    }
                    auto current = has_terms
                            ? estimate_index(collector, *entry, index_estimator_)
                            : index_candidate { *entry, {}, {} };
                    // NOTE: find operations do not provide any row order
                    if (current.result.attributes().contains(attribute::find)) {
                        return;
                    }
                    auto fixed_columns = collect_fixed_columns(current);
                    if (!find_scan_direction(*entry, *sort_keys, fixed_columns)) {
                        return;
                    }
                    if (is_better(saved, current)) {
                        saved = std::move(current);
                    }
                });
        if (!saved) {
            return false;
        }
        apply_index_to_scan(*scan, *saved, work_graph_);
        return true;
    }

    [[nodiscard]] static std::vector<column_ref> collect_fixed_columns(index_candidate const& candidate) {
        std::vector<column_ref> results {};
        auto&& keys = candidate.index.get().keys();
        for (std::size_t i = 0, n = candidate.terms.size(); i < n; ++i) {
            // NOTE: the leading equivalent terms have the single value in the range
            if (!candidate.terms[i]->equivalent()) {
                break;
            }
            results.emplace_back(keys[i].column());
        }
        return results;
    }

    [[nodiscard]] static bool is_better(std::optional<index_candidate> const& saved, index_candidate const& current) {
        if (!saved) {
            return true;
        }
        auto&& saved_result = saved->result;
        auto&& result = current.result;
        if (!saved_result.attributes().contains(attribute::single_row)
                && result.attributes().contains(attribute::single_row)) {
            return true;
        }
        return saved_result.score() < result.score();
    }
};

} // namespace

void absorb_limits(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator,
        runtime_feature_set const& features) {
    engine e { index_estimator, features };
    e.process(graph);
}

} // namespace yugawara::analyzer::details
