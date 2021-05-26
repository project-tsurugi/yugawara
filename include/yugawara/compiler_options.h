#pragma once

#include <memory>

#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/runtime_feature.h>
#include <yugawara/analyzer/index_estimator.h>

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
     * @param index_estimator the index estimator for index selection
     * @param runtime_features the supported runtime features
     */
    compiler_options( // NOLINT
            ::takatori::util::maybe_shared_ptr<::yugawara::analyzer::index_estimator const> index_estimator = {},
            runtime_feature_set runtime_features = default_runtime_features) noexcept;

    /**
     * @brief returns the available feature set of the target environment.
     * @return the available features
     */
    [[nodiscard]] runtime_feature_set& runtime_features() noexcept;

    /// @copydoc runtime_features()
    [[nodiscard]] runtime_feature_set const& runtime_features() const noexcept;

    /**
     * @brief returns the index estimator for index selection.
     * @return the index estimator
     */
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<::yugawara::analyzer::index_estimator const> index_estimator() const noexcept;

    /**
     * @brief sets the index estimator for index selection.
     * @param estimator index estimator
     * @return this
     */
    compiler_options& index_estimator(::takatori::util::maybe_shared_ptr<::yugawara::analyzer::index_estimator const> estimator) noexcept;

private:
    ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> index_estimator_ {};
    runtime_feature_set runtime_features_ { default_runtime_features };
};

} // namespace yugawara

