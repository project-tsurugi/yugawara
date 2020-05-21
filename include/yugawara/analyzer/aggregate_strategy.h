#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include <cstdlib>

#include <takatori/relation/intermediate/aggregate.h>

#include <takatori/util/enum_set.h>

namespace yugawara::analyzer {

/**
 * @brief represents kind of aggregate strategy.
 * @see aggregate_info
 */
enum class aggregate_strategy {
    /**
     * @brief group based aggregation.
     * @details This is a hint to build a `aggregate_group` operator from the corresponded `aggregate_relation`.
     */
    group = 0,

    /**
     * @brief aggregation in exchange step.
     * @details This is a hint to build `aggregate` exchange from the corresponded `join_relation`.
     *      It may require all aggregate functions are commutable.
     */
    exchange,
};

/// @brief an enum set of aggregate_strategy.
using aggregate_strategy_set = ::takatori::util::enum_set<
        aggregate_strategy,
        aggregate_strategy::group,
        aggregate_strategy::exchange>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(aggregate_strategy value) noexcept {
    using namespace std::string_view_literals;
    using kind = aggregate_strategy;
    switch (value) {
        case kind::group: return "group"sv;
        case kind::exchange: return "exchange"sv;
    }
    std::abort();
}

/**
 * @brief returns a set of available aggregate strategies for the given expression.
 * @param expression the target expression
 * @return the available strategies
 */
aggregate_strategy_set available_aggregate_strategies(::takatori::relation::intermediate::aggregate const& expression);

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
std::ostream& operator<<(std::ostream& out, aggregate_strategy value);

} // namespace yugawara::analyzer
