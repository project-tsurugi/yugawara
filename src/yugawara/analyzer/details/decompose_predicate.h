#pragma once

#include <functional>

#include <takatori/scalar/expression.h>
#include <takatori/util/ownership_reference.h>

namespace yugawara::analyzer::details {

using predicate_consumer = std::function<void(::takatori::util::ownership_reference<::takatori::scalar::expression>&&)>;

/**
 * @brief decomposes conjunctive predicate terms.
 * @details If this cannot decompose into two or more terms, this does nothing.
 * @param expression the target expression
 * @param consumer the consumer
 */
void decompose_predicate(
        ::takatori::scalar::expression& expression,
        predicate_consumer const& consumer);

/**
 * @brief decomposes conjunctive predicate terms.
 * @details If this cannot decompose into two or more terms, this just passes the input expression.
 * @param expression the target expression
 * @param consumer the consumer
 */
void decompose_predicate(
        ::takatori::util::ownership_reference<::takatori::scalar::expression> expression,
        predicate_consumer const& consumer);

} // namespace yugawara::analyzer::details
