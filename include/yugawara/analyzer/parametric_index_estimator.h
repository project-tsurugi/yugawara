#pragma once

#include "index_estimator.h"

namespace yugawara::analyzer {

/**
 * @brief an implementation of index_estimator, which estimates the selectivity from the shape of index key criteria.
 * @details This does not use any statistics: the selectivity is the product of factors for individual key criteria.
 *      The factors are given as parameters.
 *
 *      The factors of the subsequent key criteria can be weakened, for the case that the key columns are correlated:
 *      the factor of the `i`-th key criteria (0-origin) is raised to the power of `pow(decay, i)`,
 *      where `decay` is parameters_type::selectivity_decay().
 *      By default, the key criteria are regarded as independent (`decay = 1.0`).
 *
 *      If the index is unique and the search keys point a single entry
 *      (all index keys are equivalent conditions), this estimates the number of entries as `1`.
 *      Otherwise, the number of entries is unknown.
 */
class parametric_index_estimator : public index_estimator {
public:
    /**
     * @brief the parameters of parametric_index_estimator.
     */
    class parameters_type {
    public:
        /// @brief the default selectivity factor for each equivalent condition.
        static constexpr double default_equivalent_selectivity = 0.1;

        /// @brief the default selectivity factor for range condition with both bounds.
        static constexpr double default_full_bound_selectivity = 0.3;

        /// @brief the default selectivity factor for range condition with only one bound.
        static constexpr double default_half_bound_selectivity = 0.6;

        /**
         * @brief the default decay of the selectivity factors for the subsequent key criteria.
         * @details This regards the key criteria as independent, because weakening the subsequent key criteria
         *      can prefer an index with the fewer key criteria
         *      (e.g. the direct primary index prefix over the indirect secondary index with more equivalent conditions).
         */
        static constexpr double default_selectivity_decay = 1.0;

        /**
         * @brief creates a new instance with default parameters.
         */
        constexpr parameters_type() = default;

        /**
         * @brief returns the selectivity factor for each equivalent condition.
         * @return the selectivity factor
         */
        [[nodiscard]] double& equivalent_selectivity() noexcept;

        /// @copydoc equivalent_selectivity()
        [[nodiscard]] double equivalent_selectivity() const noexcept;

        /**
         * @brief returns the selectivity factor for range condition with both bounds.
         * @return the selectivity factor
         */
        [[nodiscard]] double& full_bound_selectivity() noexcept;

        /// @copydoc full_bound_selectivity()
        [[nodiscard]] double full_bound_selectivity() const noexcept;

        /**
         * @brief returns the selectivity factor for range condition with only one bound.
         * @return the selectivity factor
         */
        [[nodiscard]] double& half_bound_selectivity() noexcept;

        /// @copydoc half_bound_selectivity()
        [[nodiscard]] double half_bound_selectivity() const noexcept;

        /**
         * @brief returns the decay of the selectivity factors for the subsequent key criteria.
         * @details The factor of the `i`-th key criteria (0-origin) is raised to the power of `pow(decay, i)`.
         *      If this is `1.0`, the key criteria are regarded as independent.
         *      If this is `0.0`, only the first key criteria is considered.
         * @return the decay, in the range of `[0, 1]`
         */
        [[nodiscard]] double& selectivity_decay() noexcept;

        /// @copydoc selectivity_decay()
        [[nodiscard]] double selectivity_decay() const noexcept;

    private:
        double equivalent_selectivity_ { default_equivalent_selectivity };
        double full_bound_selectivity_ { default_full_bound_selectivity };
        double half_bound_selectivity_ { default_half_bound_selectivity };
        double selectivity_decay_ { default_selectivity_decay };
    };

    /**
     * @brief creates a new instance with default parameters.
     */
    parametric_index_estimator() = default;

    /**
     * @brief creates a new instance.
     * @param parameters the estimation parameters
     */
    explicit parametric_index_estimator(parameters_type parameters) noexcept;

    /**
     * @brief returns the estimation parameters.
     * @return the estimation parameters
     */
    [[nodiscard]] parameters_type const& parameters() const noexcept;

    [[nodiscard]] result operator()(
            storage::index const& index,
            ::takatori::util::sequence_view<search_key const> search_keys) const override;

private:
    parameters_type parameters_ {};
};

} // namespace yugawara::analyzer
