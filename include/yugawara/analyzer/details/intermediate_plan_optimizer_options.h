#pragma once

#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/runtime_feature.h>

namespace yugawara::analyzer::details {

/**
 * @brief settings for intermediate execution plan optimizer.
 */
class intermediate_plan_optimizer_options {
public:
    /**
     * @brief creates a new instance with default options.
     */
    intermediate_plan_optimizer_options() = default;

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
    [[nodiscard]] analyzer::index_estimator const& index_estimator() const noexcept;

    /**
     * @brief sets the index estimator for index selection.
     * @param estimator index estimator
     * @return this
     */
    intermediate_plan_optimizer_options& index_estimator(
            ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> estimator) noexcept;

private:
    ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> index_estimator_ {};
    runtime_feature_set runtime_features_ { runtime_feature_all };
};

} // namespace yugawara::analyzer::details
