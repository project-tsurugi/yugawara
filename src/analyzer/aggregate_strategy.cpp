#include <yugawara/analyzer/aggregate_strategy.h>

#include <yugawara/binding/aggregate_function_info.h>

namespace yugawara::analyzer {

namespace relation = ::takatori::relation;

aggregate_strategy_set available_aggregate_strategies(relation::intermediate::aggregate const& expression) {
    for (auto&& column : expression.columns()) {
        auto&& info = binding::unwrap(column.function());
        if (!info.declaration().incremental()) {
            return {
                    aggregate_strategy::group,
            };
        }
    }
    return {
            aggregate_strategy::group,
            aggregate_strategy::exchange,
    };
}

std::ostream& operator<<(std::ostream& out, aggregate_strategy value) {
    return out << to_string_view(value);
}

} // namespace yugawara::analyzer
