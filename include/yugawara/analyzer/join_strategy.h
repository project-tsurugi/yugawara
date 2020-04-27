#pragma once

#include <iostream>
#include <string>
#include <string_view>
#include <utility>

#include <cstdlib>

#include <takatori/relation/intermediate/join.h>

#include <takatori/util/enum_set.h>

namespace yugawara::analyzer {

/**
 * @brief represents kind of join strategy.
 * @see join_info
 */
enum class join_strategy : std::size_t {
    /**
     * @brief co-group based join.
     * @details This is a hint to build a `join_group` operator from the corresponded `join_relation`.
     *      This requires keys are equivalent between `lower` and `upper`, and
     *      the values in each key element must be a simple variable reference of a column in the left input.
     */
    cogroup,

    /**
     * @brief broadcast based join.
     * @details This is a hint to build `join_find` or `join_scan` operator from the corresponded `join_relation`,
     *      and then the right input will be replaced with a broadcast input.
     *      The join kind must not be a `full_outer`.
     */
    broadcast,
};

/// @brief an enum set of join_strategy.
using join_strategy_set = ::takatori::util::enum_set<
        join_strategy,
        join_strategy::cogroup,
        join_strategy::broadcast>;

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(join_strategy value) noexcept {
    using namespace std::string_view_literals;
    using kind = join_strategy;
    switch (value) {
        case kind::cogroup: return "cogroup"sv;
        case kind::broadcast: return "broadcast"sv;
    }
    std::abort();
}

/**
 * @brief returns a set of available join strategies for the given expression.
 * @param expression the target expression
 * @return the available strategies
 */
join_strategy_set available_join_strategies(::takatori::relation::intermediate::join const& expression);

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
std::ostream& operator<<(std::ostream& out, join_strategy value);

} // namespace yugawara::analyzer
