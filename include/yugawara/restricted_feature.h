#pragma once

#include <ostream>
#include <string>
#include <string_view>

#include <cstdint>
#include <cstdlib>

#include <takatori/util/enum_set.h>

namespace yugawara {

/**
 * @brief represents restricted features of runtime.
 * @attention If the compilation result includes any of these features,
 *      the compilation will just fail and report compiler_code::restricted_feature.
 * @see runtime_feature
 */
enum class restricted_feature {

    // scalar expressions

    // relation expressions

    /// @brief restrict `buffer` operator.
    relation_buffer,

    /// @brief restrict `identify` operator.
    relation_identify,

    /// @brief restrict `join_find` operator.
    relation_join_find,

    /// @brief restrict `join_scan` operator.
    relation_join_scan,

    /// @brief restrict `join` like operator with `semi-join` operations.
    relation_semi_join,

    /// @brief restrict `join` like operator with `anti-join` operations.
    relation_anti_join,

    /// @brief restrict `write` operator with `insert` operations.
    relation_write_insert,

    /// @brief restrict `values` operator.
    relation_values,

    /// @brief restrict `difference` operator.
    relation_difference,

    /// @brief restrict `flatten` operator.
    relation_flatten,

    /// @brief restrict `intersection` operator.
    relation_intersection,

    // exchanges

    /// @brief restrict `aggregate` exchange.
    exchange_aggregate,

    /// @brief restrict `broadcast` exchange.
    exchange_broadcast,

    /// @brief restrict `discard` exchange.
    exchange_discard,

    /// @brief restrict `forward` exchange.
    exchange_forward,

    /// @brief restrict `group` exchange.
    exchange_group,

    // statements

    /// @brief restrict `write` statement with delete operation.
    statement_write_delete,

    /// @brief restrict `write` statement with update operation.
    statement_write_update,
};

/**
 * @brief a set of intermediate_plan_optimizer_feature.
 */
using restricted_feature_set = ::takatori::util::enum_set<
        restricted_feature,
        restricted_feature::relation_buffer,
        restricted_feature::statement_write_update>;

/// @brief all scalar expressions of restricted_feature_set.
constexpr restricted_feature_set restricted_feature_scalar_expressions {};

/// @brief all relation expressions of restricted_feature_set.
constexpr restricted_feature_set restricted_feature_relation_expressions {
        restricted_feature::relation_buffer,
        restricted_feature::relation_identify,
        restricted_feature::relation_join_find,
        restricted_feature::relation_join_scan,
        restricted_feature::relation_semi_join,
        restricted_feature::relation_anti_join,
        restricted_feature::relation_write_insert,
        restricted_feature::relation_values,
        restricted_feature::relation_difference,
        restricted_feature::relation_flatten,
        restricted_feature::relation_intersection,
};

/// @brief all exchange steps of restricted_feature_set.
constexpr restricted_feature_set restricted_feature_exchange_steps {
        restricted_feature::exchange_aggregate,
        restricted_feature::exchange_broadcast,
        restricted_feature::exchange_discard,
        restricted_feature::exchange_forward,
        restricted_feature::exchange_group,
};

/// @brief all statements of restricted_feature_set.
constexpr restricted_feature_set restricted_feature_statements {
        restricted_feature::statement_write_delete,
        restricted_feature::statement_write_update,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
inline constexpr std::string_view to_string_view(restricted_feature value) noexcept {
    using namespace std::string_view_literals;
    using kind = restricted_feature;
    switch (value) {
        case kind::relation_buffer: return "buffer operator"sv;
        case kind::relation_identify: return "identify operator"sv;
        case kind::relation_join_find: return "join_find operator"sv;
        case kind::relation_join_scan: return "join_scan operator"sv;
        case kind::relation_semi_join: return "join like operator with semi-join operation"sv;
        case kind::relation_anti_join: return "join like operator with anti-join operation"sv;
        case kind::relation_write_insert: return "write operator with insert operation"sv;
        case kind::relation_values: return "values operator"sv;
        case kind::relation_difference: return "difference operator"sv;
        case kind::relation_flatten: return "flatten operator"sv;
        case kind::relation_intersection: return "intersection operator"sv;
        case kind::exchange_aggregate: return "aggregate exchange"sv;
        case kind::exchange_broadcast: return "broadcast exchange"sv;
        case kind::exchange_discard: return "discard exchange"sv;
        case kind::exchange_forward: return "forward exchange"sv;
        case kind::exchange_group: return "group exchange"sv;
        case kind::statement_write_delete: return "write statement with delete operation"sv;
        case kind::statement_write_update: return "write statement with update operation"sv;
    }
    std::abort();
}

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
inline std::ostream& operator<<(std::ostream& out, restricted_feature value) {
    return out << to_string_view(value);
}

} // namespace yugawara