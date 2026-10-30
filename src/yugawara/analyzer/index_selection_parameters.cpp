#include <yugawara/analyzer/index_selection_parameters.h>

namespace yugawara::analyzer {

double& index_selection_parameters::sort_cost() noexcept {
    return sort_cost_;
}

double index_selection_parameters::sort_cost() const noexcept {
    return sort_cost_;
}

double& index_selection_parameters::indirect_access_weight() noexcept {
    return indirect_access_weight_;
}

double index_selection_parameters::indirect_access_weight() const noexcept {
    return indirect_access_weight_;
}

} // namespace yugawara::analyzer
