#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdlib>

#include <takatori/util/enum_set.h>

namespace yugawara {

/**
 * @brief represents supported features of runtime.
 */
enum class runtime_feature {
    /// @brief enable any broadcast operations.
    broadcast_exchange,
    /// @brief enable aggregate operations in exchange step.
    aggregate_exchange,
    /// @brief enable index join.
    index_join,
    /// @brief enable scan on broadcast inputs (requires broadcast_exchange).
    broadcast_join_scan,
};

/**
 * @brief a set of intermediate_plan_optimizer_feature.
 */
using runtime_feature_set = ::takatori::util::enum_set<
        runtime_feature,
        runtime_feature::broadcast_exchange,
        runtime_feature::broadcast_join_scan>;

/// @brief all elements of runtime_feature_set.
constexpr runtime_feature_set runtime_feature_all {
    runtime_feature::broadcast_exchange,
    runtime_feature::aggregate_exchange,
    runtime_feature::index_join,
    runtime_feature::broadcast_join_scan,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(runtime_feature value) noexcept {
    using namespace std::string_view_literals;
    using kind = runtime_feature;
    switch (value) {
        case kind::broadcast_exchange: return "broadcast_exchange"sv;
        case kind::aggregate_exchange: return "aggregate_exchange"sv;
        case kind::index_join: return "index_join"sv;
        case kind::broadcast_join_scan: return "broadcast_join_scan"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, runtime_feature value) {
    return out << to_string_view(value);
}

} // namespace yugawara