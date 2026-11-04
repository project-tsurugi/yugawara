#pragma once

#include <cstddef>

namespace yugawara::analyzer {

/**
 * @brief the parameters to choose an index from the estimations of index_estimator.
 */
class index_selection_parameters {
public:
    /// @brief the default coefficient of the cost to sort the rows (see sort_cost()).
    static constexpr double default_sort_cost = 0.1;

    /// @brief the default weight of index accesses which also refer the primary index for individual entries.
    static constexpr double default_indirect_access_weight = 2.0;

    /// @brief the default maximum row count of limits to stop index scans early (see max_early_stop_limit_count()).
    static constexpr std::size_t default_max_early_stop_limit_count = 10'000;

    /**
     * @brief creates a new instance with default parameters.
     */
    constexpr index_selection_parameters() = default;

    /**
     * @brief returns the coefficient of the cost to sort the rows.
     * @details This is not the cost itself, but the coefficient for the selectivity of the rows to sort.
     *      If the downstream operation requires the row order, the compiler adds the cost to sort the rows
     *      to the index accesses which do not provide the row order:
     *      the cost is the estimated selectivity of the rows to sort (the ratio to the whole table)
     *      multiplied by this value.
     *      The cost to read the index entries is also the estimated selectivity (multiplied by
     *      indirect_access_weight() if the access is indirect), so that this value is the cost to sort a row
     *      relative to the cost to read a row.
     * @return the coefficient of the cost to sort the rows
     */
    [[nodiscard]] double& sort_cost() noexcept;

    /// @copydoc sort_cost()
    [[nodiscard]] double sort_cost() const noexcept;

    /**
     * @brief returns the weight of index accesses which also refer the primary index for individual entries.
     * @details The compiler multiplies the estimated selectivity of such index accesses by this value,
     *      that is, secondary index accesses which require the columns not in the index.
     * @return the weight
     */
    [[nodiscard]] double& indirect_access_weight() noexcept;

    /// @copydoc indirect_access_weight()
    [[nodiscard]] double indirect_access_weight() const noexcept;

    /**
     * @brief returns the maximum row count of limits to stop index scans early.
     * @details If the downstream limit requires the row order and has the row count,
     *      the compiler prefers the index scan which provides the row order and stops at the row count,
     *      without comparing the estimated costs.
     *      This only applies if the row count is less than or equal to this value,
     *      because a limit with a large row count rarely stops the scan early.
     *      Otherwise, the compiler compares the candidates as same as the limit without the row count.
     *
     *      Set the maximum value of `std::size_t` to apply it regardless of the row count.
     * @return the maximum row count
     */
    [[nodiscard]] std::size_t& max_early_stop_limit_count() noexcept;

    /// @copydoc max_early_stop_limit_count()
    [[nodiscard]] std::size_t max_early_stop_limit_count() const noexcept;

private:
    double sort_cost_ { default_sort_cost };
    double indirect_access_weight_ { default_indirect_access_weight };
    std::size_t max_early_stop_limit_count_ { default_max_early_stop_limit_count };
};

} // namespace yugawara::analyzer
