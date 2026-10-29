#include "remove_redundant_sort_keys.h"

#include <algorithm>
#include <utility>

#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::unsafe_downcast;

namespace {

void process(relation::intermediate::limit& expr) {
    auto&& keys = expr.sort_keys();
    auto last = keys.begin();
    for (auto it = keys.begin(); it != keys.end(); ++it) {
        auto&& variable = it->variable();
        bool redundant = std::any_of(
                keys.begin(),
                last,
                [&](auto&& kept) { return kept.variable() == variable; });
        if (redundant) {
            continue;
        }
        // NOTE: compact the kept keys in the same way as std::remove_if
        if (last != it) {
            *last = std::move(*it);
        }
        ++last;
    }
    keys.erase(last, keys.end());
}

} // namespace

void remove_redundant_sort_keys(relation::graph_type& graph) {
    for (auto&& expr : graph) {
        if (expr.kind() == relation::intermediate::limit::tag) {
            process(unsafe_downcast<relation::intermediate::limit>(expr));
        }
    }
}

} // namespace yugawara::analyzer::details
