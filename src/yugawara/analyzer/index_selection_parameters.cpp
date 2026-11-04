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

std::size_t& index_selection_parameters::max_early_stop_limit_count() noexcept {
    return max_early_stop_limit_count_;
}

std::size_t index_selection_parameters::max_early_stop_limit_count() const noexcept {
    return max_early_stop_limit_count_;
}

} // namespace yugawara::analyzer
