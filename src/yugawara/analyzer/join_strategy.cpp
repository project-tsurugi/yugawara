#include <yugawara/analyzer/join_strategy.h>

#include "details/detect_join_endpoint_style.h"

namespace yugawara::analyzer {

namespace relation = ::takatori::relation;

using details::join_endpoint_style;
using details::detect_join_endpoint_style;

join_strategy_set available_join_strategies(relation::intermediate::join const& expression) {
    static constexpr join_strategy_set all {
            join_strategy::cogroup,
            join_strategy::broadcast,
    };
    join_strategy_set result { all };

    if (expression.operator_kind() == relation::join_kind::full_outer) {
        result.erase(join_strategy::broadcast);
    }

    auto s = detect_join_endpoint_style(expression);
    if (s == join_endpoint_style::prefix || s == join_endpoint_style::range) {
        result.erase(join_strategy::cogroup);
    }

    return result;
}

std::ostream& operator<<(std::ostream& out, join_strategy value) {
    return out << to_string_view(value);
}

} // namespace yugawara::analyzer
