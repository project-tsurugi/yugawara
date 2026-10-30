#include "remove_redundant_limits.h"

#include <memory>
#include <stdexcept>
#include <vector>

#include <takatori/relation/scan.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include "ordered_relation.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

namespace {

template<class T>
bool has_start(T const& slice) {
    auto&& start = slice.start();
    if (start.is_absent()) {
        return false;
    }
    auto position = start.immediate_if();
    return !position || *position != 0;
}

bool is_empty(relation::intermediate::limit const& expr) {
    return expr.group_keys().empty()
            && expr.sort_keys().empty()
            && expr.row_slice().count().is_absent()
            && !has_start(expr.row_slice());
}

class engine {
public:
    explicit engine(runtime_feature_set const& features) noexcept :
        features_ { features }
    {}

    void process(relation::graph_type& graph) {
        std::vector<relation::intermediate::limit*> targets {};
        for (auto&& expr : graph) {
            if (expr.kind() == relation::intermediate::limit::tag) {
                auto&& limit = unsafe_downcast<relation::intermediate::limit>(expr);
                if (is_empty(limit)) {
                    targets.emplace_back(std::addressof(limit));
                }
            } else if (expr.kind() == relation::scan::tag) {
                auto&& scan = unsafe_downcast<relation::scan>(expr);
                if (auto limit = find_redundant_limit(scan); limit && !is_empty(*limit)) {
                    targets.emplace_back(limit.get());
                }
            }
        }
        for (auto* limit : targets) {
            remove(graph, *limit);
        }
    }

private:
    runtime_feature_set const& features_;

    [[nodiscard]] ::takatori::util::optional_ptr<relation::intermediate::limit> find_redundant_limit(
            relation::scan& scan) const {
        auto destination = find_ordered_destination(scan);
        if (!destination || destination->kind() != relation::intermediate::limit::tag) {
            return {};
        }
        auto&& limit = unsafe_downcast<relation::intermediate::limit>(*destination);
        if (!limit.group_keys().empty() || has_start(limit.row_slice())) {
            return {};
        }
        if (!satisfies_count(scan, limit) || !satisfies_order(scan, limit)) {
            return {};
        }
        return limit;
    }

    [[nodiscard]] bool satisfies_count(relation::scan const& scan, relation::intermediate::limit const& limit) const {
        auto&& count = limit.row_slice().count();
        if (count.is_absent()) {
            return true;
        }
        // NOTE: the scan must honor its row count
        if (!features_.contains(runtime_feature::scan_limit)) {
            return false;
        }
        auto&& slice = scan.row_slice();
        if (has_start(slice)) {
            return false;
        }
        // FIXME: currently we do not remove the limit with the variable count, even if the scan has the same one
        auto scan_count = slice.count().immediate_if();
        auto limit_count = count.immediate_if();
        return scan_count && limit_count && *scan_count <= *limit_count;
    }

    [[nodiscard]] bool satisfies_order(relation::scan const& scan, relation::intermediate::limit const& limit) const {
        if (limit.sort_keys().empty()) {
            return true;
        }
        // NOTE: the scan must provide an ordered relation in its direction
        if (!features_.contains(runtime_feature::ordered_scan)) {
            return false;
        }
        auto&& direction = scan.direction();
        return direction && find_scan_direction(scan, limit) == direction;
    }

    static void remove(relation::graph_type& graph, relation::intermediate::limit& expr) {
        auto upstream = expr.input().opposite();
        auto downstream = expr.output().opposite();
        if (!upstream || !downstream) {
            throw_exception(std::domain_error(string_builder {}
                    << "disconnected limit: "
                    << expr
                    << string_builder::to_string));
        }
        upstream->reconnect_to(*downstream);
        graph.erase(expr);
    }
};

} // namespace

void remove_redundant_limits(
        ::takatori::relation::graph_type& graph,
        runtime_feature_set const& features) {
    engine e { features };
    e.process(graph);
}

} // namespace yugawara::analyzer::details
