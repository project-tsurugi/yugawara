#include "estimate_index.h"

#include <utility>

#include <takatori/util/assertion.h>

namespace yugawara::analyzer::details {

using search_key = storage::details::search_key_element;

index_candidate estimate_index(
        scan_key_collector& collector,
        storage::index const& index,
        index_estimator const& estimator) {
    auto&& keys = index.keys();
    std::vector<scan_key_collector::term*> terms {};
    std::vector<search_key> search_keys {};
    terms.reserve(keys.size());
    search_keys.reserve(keys.size());

    for (auto&& key : keys) {
        if (auto term = collector.find(key.column())) {
            BOOST_ASSERT(term); // NOLINT

            terms.emplace_back(term.get());
            search_keys.emplace_back(term->build_index_search_key(key.column()));

            // FIXME: support more complex conditions, with pre/post filters
            if (!term->equivalent()) {
                break;
            }
        } else {
            // no more keys
            break;
        }
    }

    return index_candidate {
            index,
            estimator(
                    index,
                    search_keys,
                    collector.sort_keys(),
                    collector.referring()),
            std::move(terms),
    };
}

} // namespace yugawara::analyzer::details
