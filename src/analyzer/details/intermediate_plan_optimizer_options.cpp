#include <yugawara/analyzer/details/intermediate_plan_optimizer_options.h>

#include <yugawara/storage/configurable_provider.h>

#include "default_index_estimator.h"

namespace yugawara::analyzer::details {

intermediate_plan_optimizer_options::intermediate_plan_optimizer_options(
        ::takatori::util::object_creator creator) noexcept
    : creator_(creator)
{}

storage::provider const& intermediate_plan_optimizer_options::storage_provider() const noexcept {
    if (storage_provider_) {
        return *storage_provider_;
    }
    static storage::configurable_provider const empty;
    return empty;
}

intermediate_plan_optimizer_options& intermediate_plan_optimizer_options::storage_provider(
        std::shared_ptr<storage::provider const> provider) noexcept {
    storage_provider_ = std::move(provider);
    return *this;
}

storage::index_estimator& intermediate_plan_optimizer_options::index_estimator() const noexcept {
    if (index_estimator_) {
        return *index_estimator_;
    }
    static default_index_estimator default_estimator;
    return default_estimator;
}

intermediate_plan_optimizer_options& intermediate_plan_optimizer_options::index_estimator(
        std::shared_ptr<storage::index_estimator> estimator) noexcept {
    index_estimator_ = std::move(estimator);
    return *this;
}

::takatori::util::object_creator intermediate_plan_optimizer_options::get_object_creator() const noexcept {
    return creator_;
}

} // namespace yugawara::analyzer::details
