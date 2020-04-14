#include <yugawara/analyzer/details/intermediate_plan_optimizer_options.h>

#include <yugawara/storage/configurable_provider.h>

namespace yugawara::analyzer::details {

intermediate_plan_optimizer_options::intermediate_plan_optimizer_options(
        ::takatori::util::object_creator creator) noexcept
    : creator_(creator)
{}

storage::provider const& intermediate_plan_optimizer_options::storage_provider() const noexcept {
    if (storage_provider_ != nullptr) {
        return *storage_provider_;
    }
    static storage::configurable_provider const empty;
    return empty;
}

intermediate_plan_optimizer_options& intermediate_plan_optimizer_options::storage_provider(
        storage::provider const& provider) noexcept {
    storage_provider_ = std::addressof(provider);
    return *this;
}

::takatori::util::object_creator intermediate_plan_optimizer_options::get_object_creator() const noexcept {
    return creator_;
}

} // namespace yugawara::analyzer::details
