#pragma once

#include <unordered_map>
#include <vector>

#include <takatori/scalar/expression.h>
#include <takatori/util/object_creator.h>

#include "stream_variable_rewriter_context.h"

namespace yugawara::analyzer::details {

/**
 * @brief replaces all variable occurrences in scalar expressions.
 */
class scalar_expression_variable_rewriter {
public:
    using context_type = stream_variable_rewriter_context;

    using local_map_type = std::unordered_map<
            ::takatori::descriptor::variable,
            ::takatori::descriptor::variable,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::descriptor::variable const,
                    ::takatori::descriptor::variable>>>;

    using local_stack_type = std::vector<
            ::takatori::descriptor::variable,
            ::takatori::util::object_allocator<::takatori::descriptor::variable>>;

    explicit scalar_expression_variable_rewriter(::takatori::util::object_creator creator) noexcept;

    void operator()(context_type& context, ::takatori::scalar::expression& expr);

private:
    local_map_type locals_;
    local_stack_type stack_;
};

} // namespace yugawara::analyzer::details
