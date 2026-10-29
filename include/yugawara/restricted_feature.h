#pragma once

#include <ostream>
#include <string>
#include <string_view>

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

    /// @brief restrict `apply` operator.
    relation_apply,

    /// @brief restrict `scan` operator with forward direction.
    relation_scan_direction_forward,

    /// @brief restrict `scan` operator with backward direction.
    relation_scan_direction_backward,

    /// @brief restrict `scan` operator with row slice which has an immediate start position other than `0`.
    relation_scan_row_slice_start,

    /// @brief restrict `scan` operator with row slice which has an immediate row count.
    relation_scan_row_slice_count,

    /// @brief restrict `scan` operator with row slice which has a variable start position or row count.
    relation_scan_row_slice_variable,

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

    /**
     * @brief restrict `forward` exchange with ordering.
     * @attention this is not detected yet, until `forward` exchange supports ordering.
     */
    exchange_forward_ordering,

    /// @brief restrict `forward` exchange with row slice which has an immediate start position other than `0`.
    exchange_forward_row_slice_start,

    /// @brief restrict `forward` exchange with row slice which has an immediate row count.
    exchange_forward_row_slice_count,

    /// @brief restrict `forward` exchange with row slice which has a variable start position or row count.
    exchange_forward_row_slice_variable,

    /// @brief restrict `group` exchange with row slice which has an immediate start position other than `0`.
    exchange_group_row_slice_start,

    /// @brief restrict `group` exchange with row slice which has an immediate row count.
    exchange_group_row_slice_count,

    /// @brief restrict `group` exchange with row slice which has a variable start position or row count.
    exchange_group_row_slice_variable,

    // statements

    /// @brief restrict `write` statement with delete operation.
    statement_write_delete,

    /// @brief restrict `write` statement with update operation.
    statement_write_update,

    /// @brief restrict `alter table rename` statement.
    statement_rename_table,

    /// @brief restrict `alter index rename` statement.
    statement_rename_index,

    /// @brief restrict `alter table rename column` statement.
    statement_rename_column,

    /// @brief restrict `truncate table` statement.
    statement_truncate_table,

    /// @brief restrict `truncate table` statement with restart identity.
    statement_truncate_table_restart_identity,
};

/**
 * @brief a set of intermediate_plan_optimizer_feature.
 */
using restricted_feature_set = ::takatori::util::enum_set<
        restricted_feature,
        restricted_feature::relation_buffer,
        restricted_feature::statement_truncate_table_restart_identity>;

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
        restricted_feature::relation_apply,
        restricted_feature::relation_scan_direction_forward,
        restricted_feature::relation_scan_direction_backward,
        restricted_feature::relation_scan_row_slice_start,
        restricted_feature::relation_scan_row_slice_count,
        restricted_feature::relation_scan_row_slice_variable,
};

/// @brief all exchange steps of restricted_feature_set.
constexpr restricted_feature_set restricted_feature_exchange_steps {
        restricted_feature::exchange_aggregate,
        restricted_feature::exchange_broadcast,
        restricted_feature::exchange_discard,
        restricted_feature::exchange_forward,
        restricted_feature::exchange_group,
        restricted_feature::exchange_forward_ordering,
        restricted_feature::exchange_forward_row_slice_start,
        restricted_feature::exchange_forward_row_slice_count,
        restricted_feature::exchange_forward_row_slice_variable,
        restricted_feature::exchange_group_row_slice_start,
        restricted_feature::exchange_group_row_slice_count,
        restricted_feature::exchange_group_row_slice_variable,
};

/// @brief all statements of restricted_feature_set.
constexpr restricted_feature_set restricted_feature_statements {
        restricted_feature::statement_write_delete,
        restricted_feature::statement_write_update,
        restricted_feature::statement_rename_table,
        restricted_feature::statement_rename_index,
        restricted_feature::statement_rename_column,
        restricted_feature::statement_truncate_table,
        restricted_feature::statement_truncate_table_restart_identity,
};

/**
 * @brief returns string representation of the value.
 * @param value the target value
 * @return the corresponded string representation
 */
constexpr std::string_view to_string_view(restricted_feature value) noexcept {
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
        case kind::relation_apply: return "apply operator"sv;
        case kind::relation_scan_direction_forward: return "scan operator with forward direction"sv;
        case kind::relation_scan_direction_backward: return "scan operator with backward direction"sv;
        case kind::relation_scan_row_slice_start: return "scan operator with row slice start position"sv;
        case kind::relation_scan_row_slice_count: return "scan operator with row slice count"sv;
        case kind::relation_scan_row_slice_variable: return "scan operator with row slice variables"sv;
        case kind::exchange_aggregate: return "aggregate exchange"sv;
        case kind::exchange_broadcast: return "broadcast exchange"sv;
        case kind::exchange_discard: return "discard exchange"sv;
        case kind::exchange_forward: return "forward exchange"sv;
        case kind::exchange_group: return "group exchange"sv;
        case kind::exchange_forward_ordering: return "forward exchange with ordering"sv;
        case kind::exchange_forward_row_slice_start: return "forward exchange with row slice start position"sv;
        case kind::exchange_forward_row_slice_count: return "forward exchange with row slice count"sv;
        case kind::exchange_forward_row_slice_variable: return "forward exchange with row slice variables"sv;
        case kind::exchange_group_row_slice_start: return "group exchange with row slice start position"sv;
        case kind::exchange_group_row_slice_count: return "group exchange with row slice count"sv;
        case kind::exchange_group_row_slice_variable: return "group exchange with row slice variables"sv;
        case kind::statement_write_delete: return "write statement with delete operation"sv;
        case kind::statement_write_update: return "write statement with update operation"sv;
        case kind::statement_rename_table: return "rename table statement"sv;
        case kind::statement_rename_index: return "rename index statement"sv;
        case kind::statement_rename_column: return "rename column statement"sv;
        case kind::statement_truncate_table: return "truncate table statement"sv;
        case kind::statement_truncate_table_restart_identity: return "truncate table statement with restart identity option"sv;
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