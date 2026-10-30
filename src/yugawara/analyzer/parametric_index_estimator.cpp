#include <yugawara/analyzer/parametric_index_estimator.h>

#include <algorithm>

#include <yugawara/storage/index_feature.h>

namespace yugawara::analyzer {

using ::takatori::util::sequence_view;

double& parametric_index_estimator::parameters_type::equivalent_selectivity() noexcept {
    return equivalent_selectivity_;
}

double parametric_index_estimator::parameters_type::equivalent_selectivity() const noexcept {
    return equivalent_selectivity_;
}

double& parametric_index_estimator::parameters_type::full_bound_selectivity() noexcept {
    return full_bound_selectivity_;
}

double parametric_index_estimator::parameters_type::full_bound_selectivity() const noexcept {
    return full_bound_selectivity_;
}

double& parametric_index_estimator::parameters_type::half_bound_selectivity() noexcept {
    return half_bound_selectivity_;
}

double parametric_index_estimator::parameters_type::half_bound_selectivity() const noexcept {
    return half_bound_selectivity_;
}

parametric_index_estimator::parametric_index_estimator(parameters_type parameters) noexcept :
    parameters_ { parameters }
{}

parametric_index_estimator::parameters_type const& parametric_index_estimator::parameters() const noexcept {
    return parameters_;
}

namespace {

[[nodiscard]] bool is_unique_point(storage::index const& index, sequence_view<index_estimator::search_key const> search_keys) {
    using storage::index_feature;
    auto&& features = index.features();
    if (!features.contains(index_feature::unique) && !features.contains(index_feature::unique_constraint)) {
        return false;
    }
    if (search_keys.empty() || search_keys.size() != index.keys().size()) {
        return false;
    }
    return std::all_of(search_keys.begin(), search_keys.end(), [](auto&& key) { return key.equivalent(); });
}

} // namespace

index_estimator::result parametric_index_estimator::operator()(
        storage::index const& index,
        sequence_view<search_key const> search_keys) const {
    double selectivity = 1.0;
    for (auto&& key : search_keys) {
        if (key.equivalent()) {
            selectivity *= parameters_.equivalent_selectivity();
        } else if (key.bounded()) {
            selectivity *= parameters_.full_bound_selectivity();
        } else {
            selectivity *= parameters_.half_bound_selectivity();
        }
    }
    if (is_unique_point(index, search_keys)) {
        return result { selectivity, 1 };
    }
    return result { selectivity };
}

} // namespace yugawara::analyzer
