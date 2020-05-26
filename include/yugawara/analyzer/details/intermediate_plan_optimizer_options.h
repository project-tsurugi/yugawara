#pragma once

#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/object_creator.h>

#include <yugawara/storage/provider.h>
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
     * @brief creates a new instance with default options.
     * @param creator the object creator
     */
    explicit intermediate_plan_optimizer_options(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief returns the available feature set of the target environment.
     * @return the available features
     */
    [[nodiscard]] runtime_feature_set& runtime_features() noexcept;

    /// @copydoc runtime_features()
    [[nodiscard]] runtime_feature_set const& runtime_features() const noexcept;

    /**
     * @brief returns the storage provider.
     * @return the storage provider
     */
    [[nodiscard]] storage::provider const& storage_provider() const noexcept;

    /**
     * @brief sets the storage provider.
     * @param provider the storage provider
     * @return this
     */
    intermediate_plan_optimizer_options& storage_provider(
            ::takatori::util::maybe_shared_ptr<storage::provider const> provider) noexcept;

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

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept;

private:
    ::takatori::util::object_creator creator_ {};
    ::takatori::util::maybe_shared_ptr<storage::provider const> storage_provider_ {};
    ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> index_estimator_ {};
    runtime_feature_set runtime_features_ { runtime_feature_all };
};

} // namespace yugawara::analyzer::details
