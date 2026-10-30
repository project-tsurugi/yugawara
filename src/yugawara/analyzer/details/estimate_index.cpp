#include "estimate_index.h"

#include <algorithm>
#include <utility>

#include <takatori/util/assertion.h>
#include <takatori/util/sequence_view.h>

#include <yugawara/storage/column.h>
#include <yugawara/storage/index_feature.h>

namespace yugawara::analyzer::details {

using ::takatori::util::sequence_view;

using search_key = storage::details::search_key_element;
using access_kind = index_candidate::access_kind;
using column_ref = scan_key_collector::column_ref;

namespace {

[[nodiscard]] access_kind decide_access_kind(storage::index const& index, sequence_view<search_key const> search_keys) {
    if (search_keys.empty()) {
        return access_kind::full_scan;
    }
    if (index.features().contains(storage::index_feature::find)
            && search_keys.size() == index.keys().size()
            && std::all_of(search_keys.begin(), search_keys.end(), [](auto&& key) { return key.equivalent(); })) {
        return access_kind::find;
    }
    return access_kind::range_scan;
}

[[nodiscard]] bool is_direct(storage::index const& index, sequence_view<column_ref const> columns) {
    auto&& keys = index.keys();
    auto&& values = index.values();
    for (storage::column const& column : columns) {
        if (std::find(values.begin(), values.end(), column) != values.end()) {
            continue;
        }
        if (std::find_if(keys.begin(), keys.end(), [&](auto&& key) { return key.column() == column; }) != keys.end()) {
            continue;
        }
        return false;
    }
    return true;
}

} // namespace

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

    auto result = estimator(index, search_keys);
    auto kind = decide_access_kind(index, search_keys);
    return index_candidate {
            index,
            result,
            std::move(terms),
            kind,
            index.features().contains(storage::index_feature::scan),
            is_direct(index, collector.referring()),
    };
}

std::vector<column_ref> fixed_columns(index_candidate const& candidate) {
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

} // namespace yugawara::analyzer::details
