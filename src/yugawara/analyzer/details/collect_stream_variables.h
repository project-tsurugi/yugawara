#pragma once

#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>

#include <takatori/util/object_creator.h>

namespace yugawara::analyzer::details {

using variable_consumer = std::function<void(::takatori::descriptor::variable const&)>;

/**
 * @brief collects stream variables in the given expression.
 * @param expression the target expression
 * @param consumer the result consumer
 */
void collect_stream_variables(
        ::takatori::scalar::expression const& expression,
        variable_consumer const& consumer);

} // namespace yugawara::analyzer::details
