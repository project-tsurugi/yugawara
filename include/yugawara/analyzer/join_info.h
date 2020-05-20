#pragma once

#include <iostream>

#include "join_strategy.h"

namespace yugawara::analyzer {

/**
 * @brief a planning information about join operations.
 * @see takatori::relation::intermediate::join
 */
class join_info {
public:
    /// @brief the join strategy type.
    using strategy_type = join_strategy;

    /**
     * @brief creates a new instance, which represents nested loop join operation.
     */
    join_info() = default;

    /**
     * @brief creates a new instance.
     * @param strategy the join strategy
     */
    join_info(strategy_type strategy) noexcept; // NOLINT

    /**
     * @brief returns the join strategy.
     * @return the join strategy
     */
    [[nodiscard]] strategy_type strategy() const noexcept;

    /**
     * @brief sets the join strategy.
     * @param strategy the join strategy
     * @return this
     */
    join_info& strategy(strategy_type strategy) noexcept;

private:
    strategy_type strategy_;
};

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output stream
 */
std::ostream& operator<<(std::ostream& out, join_info const& value);

} // namespace yugawara::analyzer
