#include "decompose_prefix_match.h"

#include <takatori/type/character.h>

#include <takatori/value/character.h>

#include <takatori/scalar/dispatch.h>

#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>
#include <takatori/util/ownership_reference.h>

#include <yugawara/binding/variable_info.h>

namespace yugawara::analyzer::details {

using ::takatori::util::clone_unique;
using ::takatori::util::unsafe_downcast;

namespace {

class scalar_scanner {

public:
    void process(::takatori::util::ownership_reference<::takatori::scalar::expression> expr) {
        if (auto e = expr.find(); e && ::takatori::scalar::dispatch(*this, *e)) {
            replace(std::move(expr));
        }
    }

    [[nodiscard]] bool operator()(::takatori::scalar::expression const& expr) const {
        (void) expr;
        return false;
    }

    [[nodiscard]] bool operator()(::takatori::scalar::unary& expr) {
        process(expr.ownership_operand());
        return false;
    }

    [[nodiscard]] bool operator()(::takatori::scalar::cast& expr) {
        process(expr.ownership_operand());
        return false;
    }

    [[nodiscard]] bool operator()(::takatori::scalar::binary& expr) {
        process(expr.ownership_left());
        process(expr.ownership_right());
        return false;
    }

    [[nodiscard]] bool operator()(::takatori::scalar::compare& expr) {
        process(expr.ownership_left());
        process(expr.ownership_right());
        return false;
    }

    [[nodiscard]] bool operator()(::takatori::scalar::match& expr) {
        (void) expr;
        return true;
    }

private:
    constexpr static char pattern_wildcard_single = '_';
    constexpr static char pattern_wildcard_multiple = '%';

    void replace(::takatori::util::ownership_reference<::takatori::scalar::expression> ownership) {
        auto&& expr = unsafe_downcast<::takatori::scalar::match>(ownership.get());
        if (expr.operator_kind() != ::takatori::scalar::match::operator_kind_type::like) {
            return;
        }
        // input must be a reference to a stream variable
        if (expr.input().kind() != ::takatori::scalar::variable_reference::tag) {
            return;
        }
        auto&& input = unsafe_downcast<::takatori::scalar::variable_reference>(expr.input());
        if (binding::unwrap(input.variable()).kind() != binding::variable_info_kind::stream_variable) {
            return;
        }

        // pattern must be a string literal with a constant prefix
        auto pattern_text = get_text(expr.pattern());
        if (!pattern_text) {
            return;
        }

        // escape must be a string literal with upto ASCII 1 character
        auto escape_text = get_text(expr.escape());
        if (!escape_text || escape_text->size() > 1) {
            return;
        }
        if (escape_text->size() == 1) {
            auto c = escape_text->at(0);
            if (c < 0x20 || c > 0x7E) {
                return;
            }
        }

        auto prefix = fetch_constant_prefix(*pattern_text, *escape_text);
        if (prefix.empty()) {
            return;
        }

        auto prefix_decoded = decode_pattern(prefix, *escape_text);
        if (prefix.size() == pattern_text->size()) {
            // no wildcard, so we can use a simple match
            // -> input = prefix
            auto replacement = std::make_unique<::takatori::scalar::compare>(
                    ::takatori::scalar::comparison_operator::equal,
                    expr.release_input(),
                    create_immediate(std::move(prefix_decoded)));
            ownership.set(std::move(replacement));
            return;
        }

        bool prefix_only = true;
        for (std::size_t index = prefix.size(); index < pattern_text->size(); ++index) {
            auto c = pattern_text->at(index);
            if (c != pattern_wildcard_multiple) {
                prefix_only = false;
                break;
            }
        }
        if (prefix_only) {
            // prefix only (LIKE 'abc%'), so we can use a prefix match
            // -> prefix >= input AND input < prefix + '\xff'
            auto replacement = create_prefix_match(expr.release_input(), std::move(prefix_decoded));
            ownership.set(std::move(replacement));
            return;
        }

        // general case
        // -> prefix >= input AND input < prefix + '\xff' AND input LIKE pattern
        auto prefix_match = create_prefix_match(clone_unique(expr.input()), std::move(prefix_decoded));
        auto replacement = std::make_unique<::takatori::scalar::binary>(
                ::takatori::scalar::binary_operator::conditional_and,
                std::move(prefix_match),
                clone_unique(std::move(expr)));
        ownership.set(std::move(replacement));
    }

    [[nodiscard]] std::optional<std::string_view> get_text(::takatori::scalar::expression const& expr) const {
        if (expr.kind() != ::takatori::scalar::immediate::tag) {
            return std::nullopt;
        }
        auto&& immediate = unsafe_downcast<::takatori::scalar::immediate>(expr);
        if (immediate.value().kind() != ::takatori::value::character::tag) {
            return std::nullopt;
        }
        auto&& value = unsafe_downcast<::takatori::value::character>(immediate.value());
        return { value.get() };
    }

    [[nodiscard]] std::string_view fetch_constant_prefix(std::string_view pattern, std::string_view escape) const {
        bool saw_escape = false;
        for (std::size_t index = 0; index < pattern.size(); ++index) {
            if (saw_escape) {
                saw_escape = false;
                continue;
            }
            char c = pattern[index];
            if (!escape.empty() && c == escape[0]) {
                saw_escape = true;
                continue;
            }
            if (c == pattern_wildcard_multiple || c == pattern_wildcard_single) {
                return pattern.substr(0, index);
            }
        }
        // broken pattern
        if (saw_escape) {
            return {};
        }
        return pattern;
    }

    [[nodiscard]] std::string decode_pattern(std::string_view pattern, std::string_view escape) const {
        std::string result;
        result.reserve(pattern.size());
        bool saw_escape = false;
        for (char c : pattern) {
            if (saw_escape) {
                result.push_back(c);
                saw_escape = false;
                continue;
            }
            if (!escape.empty() && c == escape[0]) {
                saw_escape = true;
                continue;
            }
            result.push_back(c);
        }
        return result;
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> create_prefix_match(
            std::unique_ptr<::takatori::scalar::expression> input,
            std::string prefix) const {
        std::string prefix_delimited {};
        prefix_delimited.reserve(prefix.size() + 1);
        prefix_delimited.append(prefix);
        prefix_delimited.push_back('\xff');
        auto lower = std::make_unique<::takatori::scalar::compare>(
                ::takatori::scalar::comparison_operator::greater_equal,
                clone_unique(input),
                create_immediate(std::move(prefix)));
        auto upper = std::make_unique<::takatori::scalar::compare>(
                ::takatori::scalar::comparison_operator::less,
                std::move(input),
                create_immediate(std::move(prefix_delimited)));
        auto result = std::make_unique<::takatori::scalar::binary>(
                ::takatori::scalar::binary_operator::conditional_and,
                std::move(lower),
                std::move(upper));
        return result;
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::immediate> create_immediate(std::string value) const {
        return std::make_unique<::takatori::scalar::immediate>(
                ::takatori::value::character { std::move(value) },
                ::takatori::type::character { ::takatori::type::varying });
    }
};

class relation_scanner {
public:
    void process(::takatori::relation::expression& expr) {
        ::takatori::relation::intermediate::dispatch(*this, expr);
    }

    void operator()(::takatori::relation::expression const& expr) const {
        (void) expr;
    }

    void operator()(::takatori::relation::filter& expr) {
        scalars.process(expr.ownership_condition());
    }

    void operator()(::takatori::relation::intermediate::join& expr) {
        scalars.process(expr.ownership_condition());
    }

private:
    scalar_scanner scalars {};
};

} // namespace

void decompose_prefix_match(::takatori::relation::graph_type& graph) {
    relation_scanner relations {};
    for (auto&& expr : graph) {
        relations.process(expr);
    }
}

void decompose_prefix_match(::takatori::util::ownership_reference<::takatori::scalar::expression>& expr) {
    scalar_scanner scalars {};
    scalars.process(std::move(expr));
}

} // namespace yugawara::analyzer::details

