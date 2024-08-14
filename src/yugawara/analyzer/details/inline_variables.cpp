#include "inline_variables.h"

#include <takatori/scalar/dispatch.h>

#include <takatori/util/clonable.h>

namespace yugawara::analyzer::details {

namespace {

class engine {
public:
    explicit engine(inline_variables::map_type const& replacements) noexcept :
        replacements_ { replacements }
    {}

    void process(::takatori::util::ownership_reference<::takatori::scalar::expression> target) {
        if (auto expr = target.find()) {
            auto replacement = dispatch(*expr);
            if (replacement) {
                target.set(std::move(replacement));
            }
        }
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
        auto found = replacements_.find(expression.variable().reference());
        if (found == replacements_.end()) {
            return {};
        }
        return ::takatori::util::clone_unique(found->second);
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
        apply_list(expression.alternatives());
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::let& expression) {
        for (auto&& decl : expression.variables()) {
            if (auto replacement = dispatch(decl.value())) {
                decl.value(std::move(replacement));
            }
        }
        if (auto replacement = dispatch(expression.body())) {
            expression.body(std::move(replacement));
        }
        return {};
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> operator()(::takatori::scalar::function_call& expression) {
        apply_list(expression.arguments());
        return {};
    }

private:
    inline_variables::map_type const& replacements_;

    void apply_list(::takatori::tree::tree_element_vector<::takatori::scalar::expression>& list) {
        for (auto iter = list.begin(); iter != list.end(); ++iter) {
            if (auto replacement = dispatch(*iter)) {
                (void) list.exchange(iter, std::move(replacement));
            }
        }
    }
};

} // namespace

void inline_variables::reserve(std::size_t size) {
    variables_.reserve(size);
    replacements_.reserve(size);
}

void inline_variables::declare(
        ::takatori::descriptor::variable variable,
        std::unique_ptr<::takatori::scalar::expression> replacement) {
    auto key = variable.reference();
    if (replacements_.find(key) == replacements_.end()) {
        variables_.emplace_back(std::move(variable));
    }
    replacements_.emplace(key, std::move(replacement));
}

void inline_variables::apply(::takatori::util::ownership_reference<::takatori::scalar::expression> target) const {
    engine e { replacements_ };
    e.process(std::move(target));
}

} // namespace yugawara::analyzer::details
