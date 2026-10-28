#include "apply_index_to_scan.h"

#include <utility>
#include <vector>

#include <takatori/relation/find.h>

#include <takatori/util/assertion.h>
#include <takatori/util/sequence_view.h>

#include <yugawara/binding/factory.h>

#include "rewrite_criteria.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using attribute = details::index_estimator_result_attribute;

using ::takatori::util::sequence_view;

namespace {

void rewrite_as_find(
        relation::scan& expr,
        storage::index const& index,
        sequence_view<scan_key_collector::term*> terms,
        relation::graph_type& graph) {
    BOOST_ASSERT(terms.size() <= index.keys().size()); // NOLINT

    std::vector<relation::find::key> keys {};
    fill_search_key(index, keys, terms);

    binding::factory bindings {};
    auto&& find = graph.emplace<relation::find>(
            bindings(index),
            std::move(expr.columns()),
            std::move(keys));

    // reconnect
    BOOST_ASSERT(expr.output().opposite()); // NOLINT
    auto&& downstream = *expr.output().opposite();
    expr.output().disconnect_all();
    find.output().connect_to(downstream);
}

void rewrite_bounds(
        relation::scan& expr,
        storage::index const& index,
        sequence_view<scan_key_collector::term*> terms) {
    BOOST_ASSERT(terms.size() <= index.keys().size()); // NOLINT

    binding::factory bindings {};
    expr.source() = bindings(index);

    fill_endpoints(index, expr.lower(), expr.upper(), terms);
}

} // namespace

void apply_index_to_scan(
        relation::scan& expr,
        index_candidate& candidate,
        relation::graph_type& graph) {
    BOOST_ASSERT(!expr.lower() && !expr.upper()); // NOLINT
    auto&& index = candidate.index.get();
    if (candidate.result.attributes().contains(attribute::find)) {
        rewrite_as_find(expr, index, candidate.terms, graph);
        return;
    }
    rewrite_bounds(expr, index, candidate.terms);
}

} // namespace yugawara::analyzer::details
