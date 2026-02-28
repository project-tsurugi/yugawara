#include "scalar_expression_variable_rewriter.h"

#include <takatori/scalar/walk.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>

#include <yugawara/extension/scalar/extension_id.h>
#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/scalar/exists.h>

#include "rewrite_stream_variables.h"

namespace yugawara::analyzer::details {

namespace scalar = ::takatori::scalar;

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

namespace {

class engine {
public:
    explicit engine(
            scalar_expression_variable_rewriter::context_type& context,
            scalar_expression_variable_rewriter::local_map_type& locals,
            scalar_expression_variable_rewriter::local_stack_type& stack) noexcept :
        context_ { context },
        locals_ { locals },
        stack_ { stack }
    {}

    void operator()(scalar::expression const& expr) const noexcept {
        (void) expr;
    }

    bool operator()(scalar::let& expr) {
        std::size_t stack_mark = stack_.size();
        stack_.reserve(stack_.size() + expr.variables().size());
        for (auto&& decl : expr.variables()) {
            // first, visit to LHS of each variable declarator
            scalar::walk(*this, decl.value());

            // then registers the LHS and continue to the next declarator
            auto v = factory().local_variable();
            if (auto [it, success] = locals_.emplace(decl.variable(), std::move(v)); success) {
                // rewrite local variable declaration
                decl.variable() = it->second;
                // push declaration to stack
                stack_.emplace_back(it->first);
            } else {
                throw_exception(std::domain_error(string_builder {}
                        << "duplicate variable: "
                        << decl.variable()
                        << " is already bound to "
                        << it->second
                        << string_builder::to_string));
            }
        }
        scalar::walk(*this, expr.body());

        // rewind declaration stack
        while (stack_mark < stack_.size()) {
            auto&& top = stack_.back();
            locals_.erase(top);
            stack_.pop_back();
        }

        // already visited manually
        return false;
    }

    void operator()(scalar::variable_reference& expr) {
        if (auto it = locals_.find(expr.variable()); it != locals_.end()) {
            expr.variable() = it->second;
            return;
        }
        context_.rewrite_use(expr.variable());
    }

    void operator()(scalar::extension& expr) {
        switch (expr.extension_id()) {
            case extension::scalar::subquery::extension_tag:
                operator()(unsafe_downcast<extension::scalar::subquery>(expr));
                break;
            case extension::scalar::exists::extension_tag:
                operator()(unsafe_downcast<extension::scalar::exists>(expr));
                break;
            default:
                throw_exception(std::domain_error(string_builder {}
                        << "unknown extension of scalar expression: "
                        << expr.extension_id()
                        << string_builder::to_string));
        }
    }

    void operator()(extension::scalar::subquery& expr) {
        context_.rewrite_use(expr.output_column());
        rewrite_stream_variables(context_, expr.query_graph());
    }

    void operator()(extension::scalar::exists& expr) {
        rewrite_stream_variables(context_, expr.query_graph());
    }

private:
    scalar_expression_variable_rewriter::context_type& context_;
    scalar_expression_variable_rewriter::local_map_type& locals_;
    scalar_expression_variable_rewriter::local_stack_type& stack_;

    binding::factory factory() noexcept {
        return {};
    }
};

} // namespace

void scalar_expression_variable_rewriter::operator()(context_type& context, scalar::expression& expr) {
    engine e { context, locals_, stack_ };
    scalar::walk(e, expr);
    locals_.clear();
    stack_.clear();
}

} // namespace yugawara::analyzer::details
