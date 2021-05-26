#include <yugawara/analyzer/details/intermediate_plan_optimizer_options.h>

#include "default_index_estimator.h"

namespace yugawara::analyzer::details {

using ::takatori::util::maybe_shared_ptr;

runtime_feature_set& intermediate_plan_optimizer_options::runtime_features() noexcept {
    return runtime_features_;
}

runtime_feature_set const& intermediate_plan_optimizer_options::runtime_features() const noexcept {
    return runtime_features_;
}

analyzer::index_estimator const& intermediate_plan_optimizer_options::index_estimator() const noexcept {
    if (index_estimator_) {
        return *index_estimator_;
    }
    static default_index_estimator default_estimator;
    return default_estimator;
}

intermediate_plan_optimizer_options& intermediate_plan_optimizer_options::index_estimator(
        maybe_shared_ptr<analyzer::index_estimator const> estimator) noexcept {
    index_estimator_ = std::move(estimator);
    return *this;
}

} // namespace yugawara::analyzer::details
