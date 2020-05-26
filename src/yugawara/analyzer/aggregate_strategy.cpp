#include <yugawara/analyzer/aggregate_strategy.h>

#include <yugawara/binding/extract.h>

namespace yugawara::analyzer {

namespace relation = ::takatori::relation;

aggregate_strategy_set available_aggregate_strategies(relation::intermediate::aggregate const& expression) {
    for (auto&& column : expression.columns()) {
        auto&& function = binding::extract(column.function());
        if (!function.incremental()) {
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
