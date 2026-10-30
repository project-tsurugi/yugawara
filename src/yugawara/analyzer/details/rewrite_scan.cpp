#include "rewrite_scan.h"

#include <glog/logging.h>

#include <optional>
#include <utility>

#include <takatori/relation/scan.h>

#include <takatori/util/downcast.h>

#include <yugawara/storage/provider.h>

#include "apply_index_to_scan.h"
#include "compare_index_candidates.h"
#include "estimate_index.h"
#include "scan_key_collector.h"
#include "remove_orphaned_elements.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::unsafe_downcast;

namespace {

class engine {
public:
    explicit engine(index_estimator const& index_estimator, index_selection_parameters const& parameters) :
        index_estimator_ { index_estimator },
        parameters_ { parameters }
    {}

    void process(relation::graph_type& graph) {
        work_graph_.clear();
        for (auto&& expr : graph) {
            if (expr.kind() == relation::scan::tag) {
                auto&& scan = unsafe_downcast<relation::scan>(expr);
                if (!scan.row_slice() && !scan.lower() && !scan.upper()) {
                    process(scan);
                }
            }
        }
        install(graph);
    }

private:
    index_estimator const& index_estimator_;
    index_selection_parameters const& parameters_;

    relation::graph_type work_graph_ {};

    void process(relation::scan& expr) {
        // NOTE: the collector and the candidate are only valid while processing the current scan
        scan_key_collector collector {};
        if (!collector(expr, scan_key_collector::scope::scan)) {
            return;
        }
        auto&& storages = collector.table().owner();
        if (!storages) {
            // there is no index information
            return;
        }
        std::optional<index_candidate> saved {};
        storages->each_table_index(
                collector.table(),
                [&](std::string_view, std::shared_ptr<storage::index const> const& entry) {
                    auto current = estimate_index(collector, *entry, index_estimator_);
                    VLOG(50) << "index " << entry->simple_name() << ": " << current.result;
                    if (!saved || is_better_scan_candidate(current, *saved, parameters_)) {
                        saved = std::move(current);
                    }
                });
        if (saved) {
            apply_index_to_scan(expr, *saved, work_graph_);
        }
    }

    void install(relation::graph_type& graph) {
        relation::merge_into(std::move(work_graph_), graph);
        work_graph_.clear();

        remove_orphaned_elements(graph);
    }
};

} // namespace

void rewrite_scan(
        ::takatori::relation::graph_type& graph,
        class index_estimator const& index_estimator,
        index_selection_parameters const& parameters) {
    engine e { index_estimator, parameters };
    e.process(graph);
}

} // namespace yugawara::analyzer::details
