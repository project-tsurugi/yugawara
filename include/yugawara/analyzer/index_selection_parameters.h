#pragma once

namespace yugawara::analyzer {

/**
 * @brief the parameters to choose an index from the estimations of index_estimator.
 */
class index_selection_parameters {
public:
    /// @brief the default threshold of selectivity, which is regarded as a selective index access.
    static constexpr double default_strong_selectivity_threshold = 0.3;

    /// @brief the default weight of index accesses which also refer the primary index for individual entries.
    static constexpr double default_indirect_access_weight = 2.0;

    /**
     * @brief creates a new instance with default parameters.
     */
    constexpr index_selection_parameters() = default;

    /**
     * @brief returns the threshold of selectivity, which is regarded as a selective index access.
     * @details If the estimated selectivity of an index access is less than or equal to this value,
     *      the compiler prefers it to the other access that provides the row order but not selective.
     * @return the selectivity threshold
     */
    [[nodiscard]] double& strong_selectivity_threshold() noexcept;

    /// @copydoc strong_selectivity_threshold()
    [[nodiscard]] double strong_selectivity_threshold() const noexcept;

    /**
     * @brief returns the weight of index accesses which also refer the primary index for individual entries.
     * @details The compiler multiplies the estimated selectivity of such index accesses by this value,
     *      that is, secondary index accesses which require the columns not in the index.
     * @return the weight
     */
    [[nodiscard]] double& indirect_access_weight() noexcept;

    /// @copydoc indirect_access_weight()
    [[nodiscard]] double indirect_access_weight() const noexcept;

private:
    double strong_selectivity_threshold_ { default_strong_selectivity_threshold };
    double indirect_access_weight_ { default_indirect_access_weight };
};

} // namespace yugawara::analyzer
