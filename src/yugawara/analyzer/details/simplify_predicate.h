#pragma once

#include <takatori/scalar/expression.h>
#include <takatori/util/ownership_reference.h>

namespace yugawara::analyzer::details {

/**
 * @brief a result of simplify_predicate().
 */
enum class simplify_predicate_result {
    /// @brief always true.
    constant_true,
    /// @brief always false.
    constant_false,
    /// @brief always unknown.
    constant_unknown,
    /// @brief either false or unknown.
    constant_false_or_unknown,
    /// @brief either true or unknown.
    constant_true_or_unknown,
    /// @brief not sure.
    not_sure,
};

/**
 * @brief simplifies the given predicate.
 * @details This is designed for a quick way to remove redundant terms in predicates generated in push_down_predicates().
 *      This may replace each operand in the given expression if it is always evaluated as a boolean constant.
 *      This never replace the given expression itself, please treat it outside the function.
 * @param expression the target expression
 * @return the evaluation result
 */
simplify_predicate_result simplify_predicate(
        ::takatori::util::ownership_reference<::takatori::scalar::expression> expression);

} // namespace yugawara::analyzer::details
