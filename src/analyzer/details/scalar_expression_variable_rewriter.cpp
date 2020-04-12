#include "scalar_expression_variable_rewriter.h"

#include <stdexcept>

#include <takatori/scalar/walk.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>

namespace yugawara::analyzer::details {

namespace scalar = ::takatori::scalar;

using ::takatori::util::string_builder;

namespace {

class engine {
public:
    explicit engine(
            scalar_expression_variable_rewriter::context_type& context,
            scalar_expression_variable_rewriter::local_map_type& locals,
            scalar_expression_variable_rewriter::local_stack_type& stack) noexcept
        : exchange_map_(context)
        , locals_(locals)
        , stack_(stack)
    {}

    constexpr void operator()(scalar::expression&) noexcept {}

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
                throw std::domain_error(string_builder {}
                        << "duplicate variable: "
                        << decl.variable()
                        << " is already bound to "
                        << it->second
                        << string_builder::to_string);
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
        exchange_map_.rewrite_use(expr.variable());
    }

private:
    scalar_expression_variable_rewriter::context_type& exchange_map_;
    scalar_expression_variable_rewriter::local_map_type& locals_;
    scalar_expression_variable_rewriter::local_stack_type& stack_;

    binding::factory factory() noexcept {
        return binding::factory { exchange_map_.get_object_creator() };
    }
};

} // namespace

scalar_expression_variable_rewriter::scalar_expression_variable_rewriter(
        ::takatori::util::object_creator creator) noexcept
    : locals_(creator.allocator<decltype(locals_)::value_type>())
    , stack_(creator.allocator<decltype(stack_)::value_type>())
{}

void scalar_expression_variable_rewriter::operator()(context_type& context, scalar::expression& expr) {
    engine e { context, locals_, stack_ };
    scalar::walk(e, expr);
    locals_.clear();
    stack_.clear();
}

} // namespace yugawara::analyzer::details
