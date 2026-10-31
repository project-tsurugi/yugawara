#pragma once

#include <cstddef>

#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/analyzer/index_selection_parameters.h>
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

    /**
     * @brief returns the parameters to choose an index from the estimations of index_estimator().
     * @return the index selection parameters
     */
    [[nodiscard]] analyzer::index_selection_parameters& index_selection_parameters() noexcept;

    /// @copydoc index_selection_parameters()
    [[nodiscard]] analyzer::index_selection_parameters const& index_selection_parameters() const noexcept;

    /**
     * @brief returns whether to enable disjunction range hinting.
     * @return true if disjunction range hinting is enabled
     * @return false otherwise
     */
    [[nodiscard]] bool& enable_disjunction_range_hinting() noexcept;

    /// @copydoc enable_disjunction_range_hinting()
    [[nodiscard]] bool enable_disjunction_range_hinting() const noexcept;

    /**
     * @brief returns whether to inline external variables.
     * @details if disabled, the optimizer keeps external variable references as-is.
     * @return true if external variable inlining is enabled
     * @return false otherwise
     */
    [[nodiscard]] bool& enable_external_variable_inlining() noexcept;

    /// @copydoc enable_external_variable_inlining()
    [[nodiscard]] bool enable_external_variable_inlining() const noexcept;

    /**
     * @brief returns the maximum row count of the partial `limit` operators.
     * @details The optimizer places a copy of `limit` operator (partial limit) into the left input of left outer joins,
     *      only if its row count is an immediate value, and it is less than or equal to this value.
     *      The original `limit` operator is kept in the downstream of the join.
     *      If this is `0`, the optimizer never places partial limits.
     * @return the maximum row count of the partial limits
     */
    [[nodiscard]] std::size_t& max_partial_limit_count() noexcept;

    /// @copydoc max_partial_limit_count()
    [[nodiscard]] std::size_t max_partial_limit_count() const noexcept;

private:
    ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> index_estimator_ {};
    analyzer::index_selection_parameters index_selection_parameters_ {};
    runtime_feature_set runtime_features_ { runtime_feature_all };

    bool enable_disjunction_range_hinting_ {};
    bool enable_external_variable_inlining_ {};
    std::size_t max_partial_limit_count_ {};
};

} // namespace yugawara::analyzer::details
