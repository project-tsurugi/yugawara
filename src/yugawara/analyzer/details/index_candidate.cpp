#include "index_candidate.h"

#include <algorithm>
#include <cmath>
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

namespace {

// NOTE: the costs are products of the parameters, so that we compare them with a relative tolerance
constexpr double cost_tolerance = 1e-9;

[[nodiscard]] double base_cost(index_candidate const& candidate, bool use_count) {
    if (use_count) {
        return static_cast<double>(*candidate.result.count());
    }
    return candidate.result.selectivity();
}

[[nodiscard]] double weight(index_candidate const& candidate, index_selection_parameters const& parameters) {
    if (candidate.direct) {
        return 1.0;
    }
    return parameters.indirect_access_weight();
}

[[nodiscard]] candidate_comparison compare_costs(double first, double second) {
    if (std::fabs(first - second) <= cost_tolerance * std::max(std::fabs(first), std::fabs(second))) {
        return candidate_comparison::equivalent;
    }
    if (first < second) {
        return candidate_comparison::better;
    }
    return candidate_comparison::worse;
}

} // namespace

bool is_single_row(index_candidate const& candidate) noexcept {
    auto count = candidate.result.count();
    return count && *count <= 1;
}

candidate_comparison compare_index_candidates(
        index_candidate const& first,
        index_candidate const& second,
        index_selection_parameters const& parameters) {
    return compare_index_candidates(first, 1.0, second, 1.0, parameters);
}

candidate_comparison compare_index_candidates(
        index_candidate const& first,
        double first_scale,
        index_candidate const& second,
        double second_scale,
        index_selection_parameters const& parameters) {
    bool first_single = is_single_row(first);
    bool second_single = is_single_row(second);
    if (first_single != second_single) {
        return first_single ? candidate_comparison::better : candidate_comparison::worse;
    }
    // FIXME: revise how to compare the candidates if only one of them has the estimated number of entries
    bool use_count = first.result.count() && second.result.count();
    auto first_cost = base_cost(first, use_count) * weight(first, parameters) / first_scale;
    auto second_cost = base_cost(second, use_count) * weight(second, parameters) / second_scale;
    return compare_costs(first_cost, second_cost);
}

namespace {

[[nodiscard]] bool break_tie(index_candidate const& candidate, index_candidate const& best) {
    if (candidate.direct != best.direct) {
        return candidate.direct;
    }
    return candidate.index.get().simple_name() < best.index.get().simple_name();
}

[[nodiscard]] double upper_bound_cost(
        index_candidate const& candidate,
        candidate_order order,
        double sort_selectivity,
        index_selection_parameters const& parameters) {
    auto result = candidate.result.selectivity() * weight(candidate, parameters);
    if (order == candidate_order::none) {
        result += sort_selectivity * parameters.sort_cost();
    }
    return result;
}

} // namespace

bool is_better_scan_candidate(
        index_candidate const& candidate,
        index_candidate const& best,
        index_selection_parameters const& parameters) {
    switch (compare_index_candidates(candidate, best, parameters)) {
        case candidate_comparison::better: return true;
        case candidate_comparison::worse: return false;
        case candidate_comparison::equivalent: break;
    }
    return break_tie(candidate, best);
}

bool is_better_ordered_scan_candidate(
        index_candidate const& candidate,
        candidate_order order,
        index_candidate const& best,
        candidate_order best_order,
        double sort_selectivity,
        index_selection_parameters const& parameters) {
    bool single = is_single_row(candidate);
    bool best_single = is_single_row(best);
    if (single != best_single) {
        return single;
    }
    if (single) {
        return is_better_scan_candidate(candidate, best, parameters);
    }
    bool early_stop = order == candidate_order::early_stop;
    bool best_early_stop = best_order == candidate_order::early_stop;
    if (early_stop != best_early_stop) {
        return early_stop;
    }
    switch (compare_costs(
            upper_bound_cost(candidate, order, sort_selectivity, parameters),
            upper_bound_cost(best, best_order, sort_selectivity, parameters))) {
        case candidate_comparison::better: return true;
        case candidate_comparison::worse: return false;
        case candidate_comparison::equivalent: break;
    }
    return break_tie(candidate, best);
}

} // namespace yugawara::analyzer::details
