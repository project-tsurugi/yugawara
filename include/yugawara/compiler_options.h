#pragma once

#include <memory>

#include <cstddef>

#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/analyzer/index_estimator.h>
#include <yugawara/analyzer/index_selection_parameters.h>
#include <yugawara/storage/prototype_processor.h>
#include <yugawara/remote/symbol_location_query.h>

#include "runtime_feature.h"
#include "restricted_feature.h"

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
     * @brief the default list of restricted features.
     * @see restricted_features()
     */
    static constexpr restricted_feature_set default_restricted_features {};

    /**
     * @brief the default value for enabling disjunction range hinting.
     * @see enable_disjunction_range_hinting()
     */
    static constexpr bool default_enable_disjunction_range_hinting = true;

    /**
     * @brief the default value for enabling external variable inlining.
     * @see enable_external_variable_inlining()
     */
    static constexpr bool default_enable_external_variable_inlining = false;

    /**
     * @brief the default value of the maximum row count of the partial limits.
     * @see max_partial_limit_count()
     */
    static constexpr std::size_t default_max_partial_limit_count = 1000;

    /**
     * @brief creates a new instance with default options.
     * @param storage_processor the storage element prototype processor for accepting storage element definitions
     * @param index_estimator the index estimator for index selection
     * @see runtime_features()
     * @see restricted_features()
     */
    compiler_options( // NOLINT(*-explicit-constructor, *-explicit-conversions)
            ::takatori::util::maybe_shared_ptr<::yugawara::storage::prototype_processor> storage_processor,
            ::takatori::util::maybe_shared_ptr<::yugawara::analyzer::index_estimator const> index_estimator = {}) noexcept;

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
     * @see restricted_features()
     */
    [[nodiscard]] runtime_feature_set& runtime_features() noexcept;

    /// @copydoc runtime_features()
    [[nodiscard]] runtime_feature_set const& runtime_features() const noexcept;

    /**
     * @brief returns the restricted feature set of the target environment.
     * @return the restricted features
     * @see runtime_feature()
     */
    [[nodiscard]] restricted_feature_set& restricted_features() noexcept;

    /// @copydoc restricted_features()
    [[nodiscard]] restricted_feature_set const& restricted_features() const noexcept;

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

    /**
     * @brief returns a symbol location query to detect where each symbol is defined.
     * @details If this is not specified, the compiler does not compute the locations of operations,
     *      and the compilation result does not provide an operation location query.
     * @return the symbol location query
     * @return empty if the symbol location query is not specified
     * @see compiler_result::operation_location_query()
     */
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<remote::symbol_location_query const>& symbol_location_query() noexcept;

    /// @copydoc symbol_location_query()
    [[nodiscard]] ::takatori::util::maybe_shared_ptr<remote::symbol_location_query const> const& symbol_location_query() const noexcept;

private:
    runtime_feature_set runtime_features_ { default_runtime_features };
    restricted_feature_set restricted_features_ { default_restricted_features };
    ::takatori::util::maybe_shared_ptr<::yugawara::storage::prototype_processor> storage_processor_ {};
    ::takatori::util::maybe_shared_ptr<analyzer::index_estimator const> index_estimator_ {};
    analyzer::index_selection_parameters index_selection_parameters_ {};

    bool enable_disjunction_range_hinting_ { default_enable_disjunction_range_hinting };
    bool enable_external_variable_inlining_ { default_enable_external_variable_inlining };
    std::size_t max_partial_limit_count_ { default_max_partial_limit_count };
    ::takatori::util::maybe_shared_ptr<remote::symbol_location_query const> symbol_location_query_ {};
};

} // namespace yugawara

