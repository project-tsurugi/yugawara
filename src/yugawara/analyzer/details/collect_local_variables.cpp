#include "collect_local_variables.h"

#include <takatori/scalar/dispatch.h>

#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/relation/step/dispatch.h>

#include <takatori/util/clonable.h>

#include "classify_expression.h"
#include "inline_variables.h"

namespace yugawara::analyzer::details {

namespace {

class engine {
public:
    explicit engine(bool always_inline) noexcept :
        always_inline_ { always_inline }
    {}

    void process(::takatori::util::ownership_reference<::takatori::scalar::expression> target) {
        if (auto expr = target.find()) {
            if (auto replacement = dispatch(*expr)) {
                target.set(std::move(replacement));
            }
        }
    }

    void process(::takatori::relation::graph_type& target) {
        for (auto&& node : target) {
            dispatch(node);
        }
    }

    void dispatch(::takatori::relation::expression& expression) {
        if (::takatori::relation::is_available_in_intermediate_plan(expression.kind())) {
            ::takatori::relation::intermediate::dispatch(*this, expression);
        } else {
            ::takatori::relation::step::dispatch(*this, expression);
        }
    }

    void operator()(::takatori::relation::expression& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::find& expression) {
        process_search_keys(expression.keys());
    }

    void operator()(::takatori::relation::scan& expression) {
        process_search_keys(expression.lower().keys());
        process_search_keys(expression.upper().keys());
    }

    void operator()(::takatori::relation::intermediate::join& expression) {
        process_search_keys(expression.lower().keys());
        process_search_keys(expression.upper().keys());
        if (auto expr = expression.condition()) {
            if (auto replacement = dispatch(*expr)) {
                expression.condition(std::move(replacement));
            }
        }
    }

    void operator()(::takatori::relation::join_find& expression) {
        process_search_keys(expression.keys());
        if (auto expr = expression.condition()) {
            if (auto replacement = dispatch(*expr)) {
                expression.condition(std::move(replacement));
            }
        }
    }

    void operator()(::takatori::relation::join_scan& expression) {
        process_search_keys(expression.lower().keys());
        process_search_keys(expression.upper().keys());
        if (auto expr = expression.condition()) {
            if (auto replacement = dispatch(*expr)) {
                expression.condition(std::move(replacement));
            }
        }
    }

    void operator()(::takatori::relation::project& expression) {
        for (auto&& column : expression.columns()) {
            if (auto replacement = dispatch(column.value())) {
                column.value(std::move(replacement));
            }
        }
    }

    void operator()(::takatori::relation::filter& expression) {
        if (auto replacement = dispatch(expression.condition())) {
            expression.condition(std::move(replacement));
        }
    }

    void operator()(::takatori::relation::buffer& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::identify& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::intermediate::aggregate& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::intermediate::distinct& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::intermediate::limit& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::intermediate::union_& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::intermediate::intersection& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::intermediate::difference& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::emit& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::write& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::values& expression) {
        for (auto&& tuple : expression.rows()) {
            process_expression_list(tuple.elements());
        }
    }

    void operator()(::takatori::relation::intermediate::escape& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::step::join& expression) {
        if (auto expr = expression.condition()) {
            if (auto replacement = dispatch(*expr)) {
                expression.condition(std::move(replacement));
            }
        }
    }

    void operator()(::takatori::relation::step::aggregate& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::step::intersection& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::step::difference& expression) {
        (void) expression;

    }

    void operator()(::takatori::relation::step::flatten& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::step::take_flat& expression) {
        (void) expression;

    }

    void operator()(::takatori::relation::step::take_group& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::step::take_cogroup& expression) {
        (void) expression;
    }

    void operator()(::takatori::relation::step::offer& expression) {
        (void) expression;
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> dispatch(::takatori::scalar::expression& expression) {
        return ::takatori::scalar::dispatch(*this, expression);
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::expression& expression) {
        (void) expression;
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::immediate& expression) {
        (void) expression;
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::variable_reference& expression) {
        (void) expression;
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::unary& expression) {
        if (auto replacement = dispatch(expression.operand())) {
            expression.operand(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::cast& expression) {
        if (auto replacement = dispatch(expression.operand())) {
            expression.operand(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::binary& expression) {
        if (auto replacement = dispatch(expression.left())) {
            expression.left(std::move(replacement));
        }
        if (auto replacement = dispatch(expression.right())) {
            expression.right(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::compare& expression) {
        if (auto replacement = dispatch(expression.left())) {
            expression.left(std::move(replacement));
        }
        if (auto replacement = dispatch(expression.right())) {
            expression.right(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::match& expression) {
        if (auto replacement = dispatch(expression.input())) {
            expression.input(std::move(replacement));
        }
        if (auto replacement = dispatch(expression.pattern())) {
            expression.pattern(std::move(replacement));
        }
        if (auto replacement = dispatch(expression.escape())) {
            expression.escape(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::conditional& expression) {
        for (auto&& alternative : expression.alternatives()) {
            if (auto replacement = dispatch(alternative.condition())) {
                alternative.condition(std::move(replacement));
            }
            if (auto replacement = dispatch(alternative.body())) {
                alternative.body(std::move(replacement));
            }
        }
        if (auto otherwise = expression.default_expression()) {
            if (auto replacement = dispatch(*otherwise)) {
                expression.default_expression(std::move(replacement));
            }
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::coalesce& expression) {
        process_expression_list(expression.alternatives());
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::let& expression) {
        // first, process the innermost expressions
        for (auto&& decl : expression.variables()) {
            if (auto replacement = dispatch(decl.value())) {
                decl.value(std::move(replacement));
            }
        }
        if (auto replacement = dispatch(expression.body())) {
            expression.body(std::move(replacement));
        }

        // then, replace the declared variables if it is needed
        inline_variables inliner {};
        auto&& variables = expression.variables();
        for (auto iter = variables.begin(); iter != variables.end();) {
            auto&& decl = *iter;
            if (!inliner.empty()) {
                inliner.apply(decl.ownership_value());
            }
            if (is_inline_target(decl.value())) {
                inliner.declare(std::move(decl.variable()), decl.release_value());
                iter = variables.erase(iter);
                continue;
            }
            ++iter;
        }
        if (!inliner.empty()) {
            inliner.apply(expression.ownership_body());
        }
        if (expression.variables().empty()) {
            return expression.release_body();
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::function_call& expression) {
        process_expression_list(expression.arguments());
        return {};
    }

private:
    bool always_inline_ {};

    [[nodiscard]] bool is_inline_target(::takatori::scalar::expression const& expr) const {
        if (always_inline_) {
            return true;
        }
        auto classes = classify_expression(expr);
        if (classes.contains(expression_class::variable_declaration)) {
            return false;
        }
        return classes.contains(expression_class::trivial);
    }

    template<class T>
    void process_search_keys(::takatori::tree::tree_fragment_vector<T>& keys) {
        for (auto&& key : keys) {
            if (auto replacement = dispatch(key.value())) {
                key.value(std::move(replacement));
            }
        }
    }

    void process_expression_list(::takatori::tree::tree_element_vector<::takatori::scalar::expression>& list) {
        for (auto iter = list.begin(); iter != list.end(); ++iter) {
            if (auto replacement = dispatch(*iter)) {
                (void) list.exchange(iter, std::move(replacement));
            }
        }
    }
};

} // namespace

void collect_local_variables(::takatori::relation::graph_type& graph, bool always_inline) {
    engine e { always_inline };
    e.process(graph);
}

void collect_local_variables(
        ::takatori::util::ownership_reference<::takatori::scalar::expression> expression,
        bool always_inline) {
    engine e { always_inline };
    e.process(std::move(expression));
}

} // namespace yugawara::analyzer::details
