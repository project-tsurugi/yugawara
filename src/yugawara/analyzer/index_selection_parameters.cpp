#include <yugawara/analyzer/index_selection_parameters.h>

namespace yugawara::analyzer {

double& index_selection_parameters::strong_selectivity_threshold() noexcept {
    return strong_selectivity_threshold_;
}

double index_selection_parameters::strong_selectivity_threshold() const noexcept {
    return strong_selectivity_threshold_;
}

double& index_selection_parameters::indirect_access_weight() noexcept {
    return indirect_access_weight_;
}

double index_selection_parameters::indirect_access_weight() const noexcept {
    return indirect_access_weight_;
}

} // namespace yugawara::analyzer
