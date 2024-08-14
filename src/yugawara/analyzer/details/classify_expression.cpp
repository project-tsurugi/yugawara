#include "classify_expression.h"

#include <takatori/scalar/dispatch.h>

namespace yugawara::analyzer::details {

namespace {

class engine {
public:

    [[nodiscard]] expression_class_set process(::takatori::scalar::expression const& expression) {
        expression_class_set results {};
        dispatch(expression);
        if (saw_unknown_) {
            results.insert(expression_class::unknown);
        } else {
            if (!saw_not_constant_) {
                results.insert(expression_class::constant);
            }
            if (!saw_not_trivial_) {
                results.insert(expression_class::trivial);
            }
            if (!saw_not_small_) {
                results.insert(expression_class::small);
            }
            if (saw_variable_declaration_) {
                results.insert(expression_class::variable_declaration);
            }
            if (saw_function_call_) {
                results.insert(expression_class::function_call);
            }
        }
        return results;
    }

    void dispatch(::takatori::scalar::expression const& expression) {
        ::takatori::scalar::dispatch(*this, expression);
    }

    void operator()(::takatori::scalar::expression const& expression) {
        (void) expression;
        saw_unknown_ = true;
    }

    void operator()(::takatori::scalar::immediate const& expression) {
        (void) expression;
        // do nothing
    }

    void operator()(::takatori::scalar::variable_reference const& expression) {
        (void) expression;
        saw_not_constant_ = true;
    }

    void operator()(::takatori::scalar::unary const& expression) {
        dispatch(expression.operand());
    }

    void operator()(::takatori::scalar::cast const& expression) {
        dispatch(expression.operand());
        saw_not_trivial_ = true;
    }

    void operator()(::takatori::scalar::binary const& expression) {
        dispatch(expression.left());
        dispatch(expression.right());
        saw_not_trivial_ = true;
    }

    void operator()(::takatori::scalar::compare const& expression) {
        dispatch(expression.left());
        dispatch(expression.right());
        saw_not_trivial_ = true;
    }

    void operator()(::takatori::scalar::match const& expression) {
        dispatch(expression.input());
        dispatch(expression.pattern());
        dispatch(expression.escape());
        saw_not_constant_ = true;
        saw_not_trivial_ = true;
        saw_not_small_ = true;
    }

    void operator()(::takatori::scalar::conditional const& expression) {
        for (auto&& alternative : expression.alternatives()) {
            dispatch(alternative.condition());
            dispatch(alternative.body());
        }
        if (auto otherwise = expression.default_expression()) {
            dispatch(*otherwise);
        }
        saw_not_trivial_ = true;
    }

    void operator()(::takatori::scalar::coalesce const& expression) {
        for (auto&& alternative : expression.alternatives()) {
            dispatch(alternative);
        }
        saw_not_trivial_ = true;
    }

    void operator()(::takatori::scalar::let const& expression) {
        for (auto&& decl : expression.variables()) {
            dispatch(decl.value());
        }
        dispatch(expression.body());
        saw_not_trivial_ = true;
        saw_variable_declaration_ = true;
    }

    void operator()(::takatori::scalar::function_call const& expression) {
        for (auto&& arg : expression.arguments()) {
            dispatch(arg);
        }
        saw_not_constant_ = true;
        saw_not_trivial_ = true;
        saw_not_small_ = true;
        saw_function_call_ = true;
    }

private:
    bool saw_unknown_ { false };
    bool saw_not_constant_ { false };
    bool saw_not_trivial_ { false };
    bool saw_not_small_ { false };
    bool saw_variable_declaration_ { false };
    bool saw_function_call_ { false };
};

} // namespace

expression_class_set classify_expression(::takatori::scalar::expression const& expression) {
    engine e {};
    auto result = e.process(expression);
    return result;
}

} // namespace yugawara::analyzer::details
