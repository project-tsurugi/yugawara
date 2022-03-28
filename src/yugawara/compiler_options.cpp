#include <yugawara/compiler_options.h>

#include <yugawara/storage/configurable_provider.h>

namespace yugawara {

using ::takatori::util::maybe_shared_ptr;

compiler_options::compiler_options(
        runtime_feature_set  runtime_features,
        maybe_shared_ptr<storage::prototype_processor> storage_processor,
        maybe_shared_ptr<analyzer::index_estimator const> index_estimator) noexcept :
    runtime_features_ { runtime_features },
    storage_processor_ { std::move(storage_processor) },
    index_estimator_ { std::move(index_estimator) }
{}

runtime_feature_set& compiler_options::runtime_features() noexcept {
    return runtime_features_;
}

runtime_feature_set const& compiler_options::runtime_features() const noexcept {
    return runtime_features_;
}

maybe_shared_ptr<storage::prototype_processor> compiler_options::storage_processor() const noexcept {
    return storage_processor_;
}

compiler_options& compiler_options::storage_processor(maybe_shared_ptr<storage::prototype_processor> storage_processor) noexcept {
storage_processor_ = std::move(storage_processor);
return *this;
}

maybe_shared_ptr<analyzer::index_estimator const> compiler_options::index_estimator() const noexcept {
    return index_estimator_;
}

compiler_options& compiler_options::index_estimator(maybe_shared_ptr<analyzer::index_estimator const> estimator) noexcept {
    index_estimator_ = std::move(estimator);
    return *this;
}

} // namespace yugawara