#pragma once

#include <memory>

#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/runtime_feature.h>
#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/storage/prototype_processor.h>

namespace yugawara {

/**
 * @brief options of compiler.
 */
class compiler_options {
public:
    /**
     * @brief the default supported runtime features.
     * @see runtime_features()
     */
    static constexpr runtime_feature_set default_runtime_features { runtime_feature_all };

    /**
     * @brief creates a new instance with default options.
     * @param runtime_features the supported runtime features
     * @param storage_processor the storage element prototype processor for accepting storage element definitions
     * @param index_estimator the index estimator for index selection
     */
    compiler_options( // NOLINT
            runtime_feature_set runtime_features = default_runtime_features,
            ::takatori::util::maybe_shared_ptr<::yugawara::storage::prototype_processor> storage_processor = {},
            ::takatori::util::maybe_shared_ptr<::yugawara::analyzer::index_estimator const> index_estimator = {}) noexcept;

    /**
     * @brief returns the available feature set of the target environment.
     * @return the available features
     */
    [[nodiscard]] runtime_feature_set& runtime_features() noexcept;

    /// @copydoc runtime_features()
    [[nodiscard]] runtime_feature_set const& runtime_features() const noexcept;

    /**
     * @brief returns the storage element processor for handling storage element definitions.
     * @return the storage element prototype processor
     * @return empty if it is absent
     */
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<::yugawara::storage::prototype_processor> storage_processor() const noexcept;

    /**
     * @brief sets the storage element processor.
     * @param storage_processor the storage element processor.
     * @return this
     */
    compiler_options& storage_processor( ::takatori::util::maybe_shared_ptr<::yugawara::storage::prototype_processor> storage_processor) noexcept;

    /**
     * @brief returns the index estimator for index selection.
     * @return the index estimator
     * @return empty if it is absent
     */
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<::yugawara::analyzer::index_estimator const> index_estimator() const noexcept;

    /**
     * @brief sets the index estimator for index selection.
     * @param estimator index estimator
     * @return this
     */
    compiler_options& index_estimator(::takatori::util::maybe_shared_ptr<::yugawara::analyzer::index_estimator const> estimator) noexcept;

private:
    runtime_feature_set runtime_features_ { default_runtime_features };
    ::takatori::util::maybe_shared_ptr<::yugawara::storage::prototype_processor> storage_processor_ {};
    ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> index_estimator_ {};
};

} // namespace yugawara

