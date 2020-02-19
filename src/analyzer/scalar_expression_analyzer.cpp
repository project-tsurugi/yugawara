#include <yugawara/analyzer/scalar_expression_analyzer.h>

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/container/static_vector.hpp>

#include <takatori/scalar/dispatch.h>
#include <takatori/type/unknown.h>
#include <takatori/type/boolean.h>
#include <takatori/type/int.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/bit.h>
#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>

#include <yugawara/type/category.h>
#include <yugawara/type/conversion.h>
#include <yugawara/type/extensions/error.h>

#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/function_info.h>

namespace yugawara::analyzer {

namespace scalar = ::takatori::scalar;
using ::takatori::util::enum_tag;
using ::takatori::util::enum_tag_t;
using ::yugawara::type::category;
using ::yugawara::util::ternary;
using code = type_diagnostic_code;

namespace {

constexpr std::size_t short_vector_size = 16;

template<class T>
using short_vector = ::boost::container::static_vector<T, short_vector_size>;

template<class T>
using long_vector = std::vector<T, ::takatori::util::object_allocator<T>>;

constexpr std::optional<std::size_t> operator+(std::optional<std::size_t> a, std::optional<std::size_t> b) noexcept {
    if (!a || !b) return {};
    return *a + *b;
}

constexpr std::optional<std::size_t> operator-(std::optional<std::size_t> a, std::optional<std::size_t> b) noexcept {
    if (!a || !b) return {};
    if (*a < *b) return 0;
    return *a - *b;
}

constexpr std::optional<std::size_t> max(std::optional<std::size_t> a, std::optional<std::size_t> b) noexcept {
    if (!a || !b) return {};
    return std::max(*a, *b);
}

// FIXME: impl constant conversions
class engine {
public:
    using type_ptr = std::shared_ptr<::takatori::type::data const>;

    explicit engine(
            scalar_expression_analyzer& ana,
            std::vector<type_diagnostic, ::takatori::util::object_allocator<type_diagnostic>>& diagnostics,
            type::repository& repo) noexcept
        : ana_(ana)
        , diagnostics_(diagnostics)
        , repo_(repo)
    {}

    type_ptr resolve(scalar::expression const& expression) {
        if (auto resolved = ana_.find(expression)) {
            return resolved;
        }
        auto resolved = scalar::dispatch(*this, expression);
        ana_.bind(expression, resolved);
        return resolved;
    }

    type_ptr operator()(scalar::expression const&) {
        throw std::domain_error("unknown expression");
    }

    type_ptr operator()(scalar::immediate const& expr) {
        return expr.shared_type();
    }

    type_ptr operator()(scalar::variable_reference const& expr) {
        if (auto type = ana_.find(expr.variable())) {
            return type;
        }
        return raise({
                code::unresolved_variable,
                expr,
                repo_.get(::takatori::type::unknown()),
                {},
        });
    }

    type_ptr operator()(scalar::unary const& expr) {
        auto operand = resolve(expr.operand());
        return scalar::dispatch(*this, expr.operator_kind(), expr, std::move(operand));
    }

    type_ptr operator()(scalar::cast const& expr) {
        auto operand = resolve(expr.operand());
        switch (type::is_cast_convertible(*operand, expr.type())) {
            case ternary::yes: return expr.shared_type();
            case ternary::no: return raise({
                    code::unsupported_type,
                    expr,
                    std::move(operand),
                    { type::category_of(expr.type()) },
            });
            case ternary::unknown: return operand;
        }
        std::abort();
    }

    type_ptr operator()(scalar::binary const& expr) {
        auto left = resolve(expr.left());
        auto right = resolve(expr.right());
        return scalar::dispatch(*this, expr.operator_kind(), expr, std::move(left), std::move(right));
    }

    type_ptr operator()(scalar::compare const& expr) {
        auto left = resolve(expr.left());
        auto right = resolve(expr.right());
        auto lcat = type::category_of(*left);
        auto rcat = type::category_of(*right);
        if (lcat == category::unresolved || rcat == category::unresolved) {
            return repo_.get(::takatori::type::boolean());
        }
        auto unify = type::unifying_conversion(*left, *right);
        auto ucat = type::category_of(*unify);
        if (ucat == category::unresolved) {
            report({
                    code::inconsistent_type,
                    expr.right(),
                    std::move(right),
                    { rcat },
            });
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(scalar::match const& expr) {
        std::array<std::reference_wrapper<::takatori::scalar::expression const>, 3> properties {
                expr.input(),
                expr.pattern(),
                expr.escape(),
        };
        for (auto property : properties) {
            auto ptype = resolve(property);
            switch (type::category_of(*ptype)) {
                case category::unresolved: // NG but continue
                case category::unknown:
                case category::character_string:
                    break; // ok
                default:
                    report({
                            code::unsupported_type,
                            property,
                            std::move(ptype),
                            { category::character_string },
                    });
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(scalar::conditional const& expr) {
        type_ptr current = repo_.get(::takatori::type::unknown {});
        for (auto&& alternative : expr.alternatives()) {
            auto condition = resolve(alternative.condition());
            switch (type::category_of(*condition)) {
                case category::unresolved: // NG but continue
                case category::unknown:
                case category::boolean:
                    break; // ok
                default:
                    report({
                            code::unsupported_type,
                            alternative.condition(),
                            std::move(condition),
                            { category::boolean },
                    });
            }
            auto body = resolve(alternative.body());
            if (type::category_of(*body) == category::unresolved) return body;
            auto next = type::unifying_conversion(*current, *body);
            if (type::category_of(*next) == category::unresolved) {
                return raise({
                        code::inconsistent_type,
                        alternative.body(),
                        std::move(body),
                        { type::category_of(*current) },
                });
            }
            current = std::move(next);
        }
        if (auto e = expr.default_expression()) {
            auto body = resolve(*e);
            if (type::category_of(*body) == category::unresolved) return body;
            auto next = type::unifying_conversion(*current, *body);
            if (type::category_of(*next) == category::unresolved) {
                return raise({
                        code::inconsistent_type,
                        *e,
                        std::move(body),
                        { type::category_of(*current) },
                });
            }
            current = std::move(next);
        }
        return current;
    }

    type_ptr operator()(scalar::coalesce const& expr) {
        type_ptr current = repo_.get(::takatori::type::unknown {});
        for (auto&& alternative : expr.alternatives()) {
            auto alt = resolve(alternative);
            if (type::category_of(*alt) == category::unresolved) return alt;
            auto next = type::unifying_conversion(*current, *alt);
            if (type::category_of(*next) == category::unresolved) {
                return raise({
                        code::inconsistent_type,
                        alternative,
                        std::move(alt),
                        { type::category_of(*current) },
                });
            }
            current = std::move(next);
        }
        return current;
    }

    type_ptr operator()(scalar::let const& expr) {
        for (auto&& decl : expr.variables()) {
            auto def = resolve(decl.value());
            ana_.bind(decl.variable(), std::move(def));
        }
        return resolve(expr.body());
    }

    type_ptr operator()(scalar::function_call const& expr) {
        // type resolution has been completed since overload was detected,
        // we just return the function result type (even if it is unresolved)
        auto&& func = binding::extract(expr.function());
        return func.declaration().shared_return_type();
    }

    // FIXME: more expressions

    type_ptr operator()(enum_tag_t<scalar::unary_operator::plus>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
                return raise({
                        code::ambiguous_type,
                        expr.operand(),
                        std::move(operand),
                        { category::number, category::time_interval },
                });
            case category::number:
                return type::unary_numeric_promotion(*operand, repo_);
            case category::time_interval:
                return type::unary_time_interval_promotion(*operand, repo_);
            case category::unresolved:
                return operand;
            default:
                return raise({
                        code::unsupported_type,
                        expr.operand(),
                        std::move(operand),
                        { category::number, category::time_interval },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::sign_inversion>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
                return raise({
                        code::ambiguous_type,
                        expr.operand(),
                        std::move(operand),
                        { category::number, category::time_interval },
                });
            case category::number:
                return type::unary_numeric_promotion(*operand, repo_);
            case category::time_interval:
                return type::unary_time_interval_promotion(*operand, repo_);
            case category::unresolved:
                return operand;
            default:
                return raise({
                        code::unsupported_type,
                        expr.operand(),
                        std::move(operand),
                        { category::number, category::time_interval },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::length>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::character_string:
            case category::bit_string:
            case category::unresolved:
                break; // ok
            case category::unknown:
                report({
                        code::ambiguous_type,
                        expr.operand(),
                        std::move(operand),
                        { category::character_string, category::bit_string },
                });
                break;
            default:
                report({
                        code::unsupported_type,
                        expr.operand(),
                        std::move(operand),
                        { category::character_string, category::bit_string },
                });
                break;
        }
        return repo_.get(::takatori::type::int4());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::conditional_not>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
            case category::boolean:
            case category::unresolved:
                break; // ok
            default:
                report({
                        code::unsupported_type,
                        expr.operand(),
                        std::move(operand),
                        { category::boolean },
                });
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_null>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
            case category::boolean:
            case category::number:
            case category::character_string:
            case category::bit_string:
            case category::temporal:
            case category::time_interval:
            case category::unresolved:
                break; // ok
            default:
                report({
                        code::unsupported_type,
                        expr.operand(),
                        std::move(operand),
                        {
                                category::boolean,
                                category::number,
                                category::character_string,
                                category::bit_string,
                                category::temporal,
                                category::time_interval,
                        },
                });
                break;
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_true>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
            case category::boolean:
            case category::unresolved:
                break; // ok
            default:
                report({
                        code::unsupported_type,
                        expr.operand(),
                        std::move(operand),
                        { category::boolean },
                });
                break;
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_false>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
            case category::boolean:
            case category::unresolved:
                break;
            default:
                report({
                        code::unsupported_type,
                        expr.operand(),
                        std::move(operand),
                        { category::boolean },
                });
                break;
        }
        return repo_.get(::takatori::type::boolean());
    }

    static std::pair<category, category> category_pair(type_ptr const& left, type_ptr const& right) noexcept {
        auto lcat = type::category_of(*left);
        auto rcat = type::category_of(*right);
        if (lcat == category::unknown) {
            return { rcat, rcat };
        }
        if (rcat == category::unknown) {
            return { lcat, lcat };
        }
        return { lcat, rcat };
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::add>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                // FIXME: unknown * number
                return raise({
                        code::ambiguous_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::temporal, category::time_interval },
                });
            case category::number:
                // FIXME: number * unknown
                // FIXME: as decimal
                // binary numeric -> if decimal -> compute prec/scale
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                            && right->kind() == ::takatori::type::type_kind::decimal) {
                        auto&& ld = ::takatori::util::downcast<::takatori::type::decimal>(*left);
                        auto&& rd = ::takatori::util::downcast<::takatori::type::decimal>(*right);
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(max(p-s, q-t) + max(s,t) + 1, max(s,t))
                        return repo_.get(::takatori::type::decimal(
                                max(ld.precision() - ld.scale(), rd.precision() - rd.scale()) + std::max(ld.scale(), rd.scale()) + 1,
                                std::max(ld.scale(), rd.scale())));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::number },
                });
            case category::temporal:
                if (rcat == category::time_interval) return type::unary_temporal_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::time_interval },
                });
            case category::time_interval:
                if (rcat == category::temporal) return type::unary_temporal_promotion(*right, repo_);
                if (rcat == category::time_interval) return type::binary_time_interval_promotion(*left, *right, repo_);
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::temporal, category::time_interval },
                });
            default:
                return raise({
                        code::unsupported_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::temporal, category::time_interval },
                 });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::subtract>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                return raise({
                        code::ambiguous_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::temporal, category::time_interval },
                });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        auto&& ld = ::takatori::util::downcast<::takatori::type::decimal>(*left);
                        auto&& rd = ::takatori::util::downcast<::takatori::type::decimal>(*right);
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(max(p-s, q-t) + max(s,t) + 1, max(s,t))
                        return repo_.get(::takatori::type::decimal(
                                max(ld.precision() - ld.scale(), rd.precision() - rd.scale()) + std::max(ld.scale(), rd.scale()) + 1,
                                std::max(ld.scale(), rd.scale())));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::number },
                });
            case category::temporal:
                if (rcat == category::time_interval) return type::unary_temporal_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::time_interval },
                });
            case category::time_interval:
                // NOTE: <time_interval> - <temporal> is not defined
                if (rcat == category::time_interval) return type::binary_time_interval_promotion(*left, *right, repo_);
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::time_interval },
                });
            default:
                return raise({
                        code::unsupported_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::temporal, category::time_interval },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::multiply>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                return raise({
                        code::ambiguous_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::time_interval },
                });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        auto&& ld = ::takatori::util::downcast<::takatori::type::decimal>(*left);
                        auto&& rd = ::takatori::util::downcast<::takatori::type::decimal>(*right);
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(p + q, s + t)
                        return repo_.get(::takatori::type::decimal(
                                ld.precision() + rd.precision(),
                                ld.scale() + rd.scale()));
                    }
                    return result;
                }
                if (rcat == category::time_interval) return type::unary_time_interval_promotion(*right, repo_);
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::number, category::time_interval },
                });
            case category::time_interval:
                if (rcat == category::number) return type::unary_time_interval_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::time_interval },
                });
            default:
                return raise({
                        code::unsupported_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::time_interval },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::divide>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                return raise({
                        code::ambiguous_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::time_interval },
                });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        auto&& ld = ::takatori::util::downcast<::takatori::type::decimal>(*left);
                        auto&& rd = ::takatori::util::downcast<::takatori::type::decimal>(*right);
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(p + q, s + t)
                        return repo_.get(::takatori::type::decimal(
                                ld.precision() + rd.precision(),
                                ld.scale() + rd.scale()));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::number },
                });
            case category::time_interval:
                if (rcat == category::number) return type::unary_time_interval_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::number },
                });
            default:
                return raise({
                        code::unsupported_type,
                        expr.left(),
                        std::move(left),
                        { category::number, category::time_interval },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::remainder>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        return operator()(enum_tag<scalar::binary_operator::divide>, expr, std::move(left), std::move(right));
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::concat>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                return raise({
                        code::ambiguous_type,
                        expr.left(),
                        std::move(left),
                        { category::character_string, category::bit_string },
                });
            case category::character_string:
                if (rcat == category::character_string) {
                    auto result = type::binary_character_string_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::character
                            && right->kind() == ::takatori::type::type_kind::character) {
                        auto&& lv = ::takatori::util::downcast<::takatori::type::character>(*left);
                        auto&& rv = ::takatori::util::downcast<::takatori::type::character>(*right);
                        auto&& ret = ::takatori::util::downcast<::takatori::type::character>(*result);
                        return repo_.get(::takatori::type::character(
                                ::takatori::type::varying_t(ret.varying()),
                                lv.length() + rv.length()));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::character_string },
                });
            case category::bit_string:
                if (rcat == category::bit_string) {
                    auto result = type::binary_bit_string_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::bit
                            && right->kind() == ::takatori::type::type_kind::bit) {
                        auto&& lv = ::takatori::util::downcast<::takatori::type::bit>(*left);
                        auto&& rv = ::takatori::util::downcast<::takatori::type::bit>(*right);
                        auto&& ret = ::takatori::util::downcast<::takatori::type::bit>(*result);
                        return repo_.get(::takatori::type::bit(
                                ::takatori::type::varying_t(ret.varying()),
                                lv.length() + rv.length()));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        expr.right(),
                        std::move(right),
                        { category::bit_string },
                });
            case category::collection:
                // FIXME: impl array like
            default:
                return raise({
                        code::unsupported_type,
                        expr.left(),
                        std::move(left),
                        { category::character_string, category::bit_string },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::conditional_and>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved || rcat == category::unresolved) return repo_.get(::takatori::type::boolean());
        switch (lcat) {
            case category::unknown:
            case category::boolean:
                if (rcat != category::unknown && rcat != category::boolean) {
                    report({
                            code::inconsistent_type,
                            expr.right(),
                            std::move(right),
                            { category::boolean },
                    });
                }
                break;
            default:
                report({
                        code::unsupported_type,
                        expr.left(),
                        std::move(left),
                        { category::boolean },
                });
                break;
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::conditional_or>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        return operator()(enum_tag<scalar::binary_operator::conditional_and>, expr, std::move(left), std::move(right));
    }

private:
    scalar_expression_analyzer& ana_;
    std::vector<type_diagnostic, ::takatori::util::object_allocator<type_diagnostic>>& diagnostics_;
    type::repository& repo_;

    void report(type_diagnostic diagnostic) {
        diagnostics_.emplace_back(std::move(diagnostic));
    }

    type_ptr raise(type_diagnostic diagnostic) {
        report(std::move(diagnostic));
        static auto result = std::make_shared<type::extensions::error>();
        return result;
    }
};

} // namespace

scalar_expression_analyzer::scalar_expression_analyzer(::takatori::util::object_creator creator) noexcept
    : variables_(creator.allocator<decltype(variables_)::value_type>())
    , expressions_(creator.allocator<decltype(expressions_)::value_type>())
    , diagnostics_(creator.allocator<decltype(diagnostics_)::value_type>())
{}

std::shared_ptr<::takatori::type::data const> scalar_expression_analyzer::find(
        ::takatori::scalar::expression const& expression) const {
    if (auto it = expressions_.find(std::addressof(expression)); it != expressions_.end()) {
        return it->second;
    }
    return {};
}

scalar_expression_analyzer& scalar_expression_analyzer::bind(
        ::takatori::scalar::expression const& expression,
        std::shared_ptr<::takatori::type::data const> type) {
    auto&& type_ref = *type;
    if (auto [iter, success] = expressions_.emplace(std::addressof(expression), std::move(type)); !success) {
        // already exists
        if (*iter->second != type_ref) {
            throw std::domain_error("rebind different type for the expression");
        }
    }
    return *this;
}

std::shared_ptr<::takatori::type::data const> scalar_expression_analyzer::find(
        ::takatori::descriptor::variable const& variable) const {
    if (auto it = variables_.find(variable); it != variables_.end()) {
        return it->second;
    }
    return {};
}

scalar_expression_analyzer& scalar_expression_analyzer::bind(
        ::takatori::descriptor::variable const& variable,
        std::shared_ptr<::takatori::type::data const> type) {
    auto&& type_ref = *type;
    if (auto [iter, success] = variables_.emplace(variable, std::move(type)); !success) {
        // already exists
        if (*iter->second != type_ref) {
            throw std::domain_error("rebind different type for the variable");
        }
    }
    return *this;
}

scalar_expression_analyzer& scalar_expression_analyzer::bind(
        ::takatori::descriptor::variable const& variable,
        ::takatori::type::data&& type) {
    return bind(variable, ::takatori::util::clone_shared(std::move(type)));
}

std::shared_ptr<::takatori::type::data const> scalar_expression_analyzer::resolve(
        ::takatori::scalar::expression const& expression,
        type::repository& repo) {
    return engine { *this, diagnostics_, repo }
        .resolve(expression);
}

bool scalar_expression_analyzer::has_diagnostics() const noexcept {
    return !diagnostics_.empty();
}

::takatori::util::sequence_view<type_diagnostic const> scalar_expression_analyzer::diagnostics() const noexcept {
    return diagnostics_;
}

void scalar_expression_analyzer::clear_diagnostics() noexcept {
    diagnostics_.clear();
}

takatori::util::object_creator scalar_expression_analyzer::get_object_creator() const {
    return expressions_.get_allocator();
}

} // namespace yugawara::analyzer
