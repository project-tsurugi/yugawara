#pragma once

#include <ostream>

#include "aggregate_strategy.h"

namespace yugawara::analyzer {

/**
 * @brief a planning information about aggregate operations.
 * @see takatori::relation::intermediate::aggregate
 */
class aggregate_info {
public:
    /// @brief the aggregate strategy type.
    using strategy_type = aggregate_strategy;

    /**
     * @brief creates a new instance, which represents group based aggregate operation.
     */
    aggregate_info() = default;

    /**
     * @brief creates a new instance.
     * @param strategy the aggregate strategy
     */
    aggregate_info(strategy_type strategy) noexcept; // NOLINT

    /**
     * @brief returns the aggregate strategy.
     * @return the aggregate strategy
     */
    [[nodiscard]] strategy_type strategy() const noexcept;

    /**
     * @brief sets the aggregate strategy.
     * @param strategy the aggregate strategy
     * @return this
     */
    aggregate_info& strategy(strategy_type strategy) noexcept;

private:
    strategy_type strategy_;
};

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output stream
 */
std::ostream& operator<<(std::ostream& out, aggregate_info const& value);

} // namespace yugawara::analyzer
