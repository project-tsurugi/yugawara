#include <yugawara/compiler_options.h>

#include <yugawara/storage/configurable_provider.h>

namespace yugawara {

compiler_options::compiler_options(
        ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> index_estimator,
        runtime_feature_set  runtime_features,
        ::takatori::util::object_creator creator) noexcept
    : creator_(creator)
    , index_estimator_(std::move(index_estimator))
    , runtime_features_(runtime_features)
{}

runtime_feature_set& compiler_options::runtime_features() noexcept {
    return runtime_features_;
}

runtime_feature_set const& compiler_options::runtime_features() const noexcept {
    return runtime_features_;
}

::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> compiler_options::index_estimator() const noexcept {
    return index_estimator_;
}

compiler_options& compiler_options::index_estimator(
        ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> estimator) noexcept {
    index_estimator_ = std::move(estimator);
    return *this;
}

::takatori::util::object_creator compiler_options::get_object_creator() const noexcept {
    return creator_;
}

} // namespace yugawara