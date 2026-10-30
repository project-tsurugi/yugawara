#pragma once

#include "index_estimator.h"

namespace yugawara::analyzer {

/**
 * @brief an implementation of index_estimator, which estimates the selectivity from the shape of index key criteria.
 * @details This does not use any statistics: the selectivity is the product of factors for individual key criteria,
 *      that is, this regards the individual key criteria as independent.
 *      The factors are given as parameters.
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

    private:
        double equivalent_selectivity_ { default_equivalent_selectivity };
        double full_bound_selectivity_ { default_full_bound_selectivity };
        double half_bound_selectivity_ { default_half_bound_selectivity };
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
