#pragma once

#include <vector>

#include <tsl/hopscotch_map.h>

#include <takatori/scalar/expression.h>

#include "stream_variable_rewriter_context.h"

namespace yugawara::analyzer::details {

/**
 * @brief replaces all variable occurrences in scalar expressions.
 */
class scalar_expression_variable_rewriter {
public:
    using context_type = stream_variable_rewriter_context;

    using local_map_type = ::tsl::hopscotch_map<
            ::takatori::descriptor::variable,
            ::takatori::descriptor::variable,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>>;

    using local_stack_type = std::vector<::takatori::descriptor::variable>;

    void operator()(context_type& context, ::takatori::scalar::expression& expr);

private:
    local_map_type locals_;
    local_stack_type stack_;
};

} // namespace yugawara::analyzer::details
