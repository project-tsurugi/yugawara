#include "compare_index_candidates.h"

#include <algorithm>
#include <cmath>

namespace yugawara::analyzer::details {

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

bool is_better_scan_candidate(
        index_candidate const& candidate,
        index_candidate const& best,
        index_selection_parameters const& parameters) {
    switch (compare_index_candidates(candidate, best, parameters)) {
        case candidate_comparison::better: return true;
        case candidate_comparison::worse: return false;
        case candidate_comparison::equivalent: break;
    }
    if (candidate.direct != best.direct) {
        return candidate.direct;
    }
    return candidate.index.get().simple_name() < best.index.get().simple_name();
}

} // namespace yugawara::analyzer::details
