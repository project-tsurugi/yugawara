#include <yugawara/analyzer/expression_analyzer.h>

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/container/static_vector.hpp>

#include <tsl/hopscotch_set.h>

#include <takatori/scalar/dispatch.h>
#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/relation/step/dispatch.h>
#include <takatori/plan/dispatch.h>

#include <takatori/type/unknown.h>
#include <takatori/type/boolean.h>
#include <takatori/type/int.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/bit.h>
#include <takatori/util/downcast.h>

#include <yugawara/type/category.h>
#include <yugawara/type/conversion.h>
#include <yugawara/type/extensions/error.h>

#include <yugawara/binding/table_column_info.h>
#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/external_variable_info.h>
#include <yugawara/binding/function_info.h>
#include <yugawara/binding/aggregate_function_info.h>
#include <yugawara/binding/relation_info.h>
#include <yugawara/binding/index_info.h>

#include <yugawara/storage/table.h>

namespace yugawara::analyzer {

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::util::enum_tag;
using ::takatori::util::enum_tag_t;
using ::takatori::util::unsafe_downcast;
using ::yugawara::type::category;
using ::yugawara::type::extensions::is_error;
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

template<class T>
constexpr T& unwrap(std::reference_wrapper<T> wrapper) noexcept {
    return wrapper;
}

// FIXME: impl constant conversions
class engine {
public:
    using type_ptr = std::shared_ptr<::takatori::type::data const>;

    using saw_set = ::tsl::hopscotch_set<
            relation::expression const*,
            std::hash<relation::expression const*>,
            std::equal_to<>>;

    explicit engine(
            expression_analyzer& ana,
            bool validate,
            std::vector<type_diagnostic, ::takatori::util::object_allocator<type_diagnostic>>& diagnostics,
            type::repository& repo) noexcept
        : ana_(ana)
        , diagnostics_(diagnostics)
        , repo_(repo)
        , validate_(validate)
    {}

    type_ptr resolve(scalar::expression const& expression) {
        if (auto resolved = ana_.expressions().find(expression)) {
            return resolved;
        }
        auto resolved = scalar::dispatch(*this, expression);
        ana_.expressions().bind(expression, resolved, true);
        return resolved;
    }

    bool resolve(relation::expression const& expression, bool recursive) {
        if (recursive) {
            saw_set saw {};
            return resolve_recursive(expression, saw);
        }
        return resolve_flat(expression);
    }

    bool resolve_recursive(
            relation::expression const& expression,
            saw_set& saw) {
        if (saw.find(std::addressof(expression)) != saw.end()) {
            // already visited
            return true;
        }
        for (auto&& port : expression.input_ports()) {
            if (auto opposite = port.opposite()) {
                if (!resolve_recursive(opposite->owner(), saw)) {
                    return false;
                }
            } else {
                throw std::invalid_argument("incomplete relational expression");
            }
        }
        saw.emplace(std::addressof(expression));
        return resolve_flat(expression);
    }

    bool resolve_flat(relation::expression const& expression) {
        if (relation::is_available_in_step_plan(expression.kind())) {
            return relation::step::dispatch(*this, expression);
        }
        assert(relation::is_available_in_intermediate_plan(expression.kind())); // NOLINT
        return relation::intermediate::dispatch(*this, expression);
    }

    bool resolve(plan::step const& step, bool recursive) {
        if (recursive) {
            std::unordered_set<plan::step const*> saw {};
            if (step.kind() == plan::step_kind::process) {
                return resolve_recursive(unsafe_downcast<plan::process>(step), saw);
            }
            return resolve_recursive(unsafe_downcast<plan::exchange>(step), saw);
        }
        return plan::dispatch(*this, step);
    }

    template<class Step>
    bool resolve_recursive(Step const& step, std::unordered_set<plan::step const*> saw) {
        if (saw.find(std::addressof(step)) != saw.end()) {
            // already visited
            return true;
        }
        for (auto&& upstream : step.upstreams()) {
            if (!resolve_recursive(upstream, saw)) {
                return false;
            }
        }
        saw.emplace(std::addressof(step));
        return plan::dispatch(*this, step);
    }

    type_ptr operator()(scalar::expression const&) {
        throw std::domain_error("unknown expression");
    }

    bool operator()(relation::expression const&) {
        throw std::domain_error("unknown expression");
    }

    bool operator()(plan::step const&) {
        throw std::domain_error("unknown step");
    }

    type_ptr operator()(scalar::immediate const& expr) {
        return expr.shared_type();
    }

    type_ptr operator()(scalar::variable_reference const& expr) {
        if (auto&& resolution = ana_.variables().find(expr.variable())) {
            if (auto type = ana_.inspect(resolution)) {
                return type;
            }
        }
        return raise({
                code::unresolved_variable,
                extract_region(expr),
                repo_.get(::takatori::type::unknown()),
                {},
        });
    }

    type_ptr operator()(scalar::unary const& expr) {
        auto operand = resolve(expr.operand());
        return scalar::dispatch(*this, expr.operator_kind(), expr, std::move(operand));
    }

    type_ptr operator()(scalar::cast const& expr) {
        if (validate_) {
            auto operand = resolve(expr.operand());
            if (type::is_cast_convertible(*operand, expr.type()) == ternary::no) {
                report({
                        code::unsupported_type,
                        extract_region(expr),
                        std::move(operand),
                        { type::category_of(expr.type()) },
                });
            }
        }
        return expr.shared_type();
    }

    type_ptr operator()(scalar::binary const& expr) {
        auto left = resolve(expr.left());
        auto right = resolve(expr.right());
        return scalar::dispatch(*this, expr.operator_kind(), expr, std::move(left), std::move(right));
    }

    type_ptr operator()(scalar::compare const& expr) {
        if (validate_) {
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
                        extract_region(expr.right()),
                        std::move(right),
                        { rcat },
                });
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(scalar::match const& expr) {
        if (validate_) {
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
                                extract_region(unwrap(property)),
                                std::move(ptype),
                                { category::character_string },
                        });
                }
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(scalar::conditional const& expr) {
        type_ptr current = repo_.get(::takatori::type::unknown {});
        for (auto&& alternative : expr.alternatives()) {
            if (validate_) {
                auto condition = resolve(alternative.condition());
                switch (type::category_of(*condition)) {
                    case category::unresolved: // NG but continue
                    case category::unknown:
                    case category::boolean:
                        break; // ok
                    default:
                        report({
                                code::unsupported_type,
                                extract_region(alternative.condition()),
                                std::move(condition),
                                { category::boolean },
                        });
                }
            }
            auto body = resolve(alternative.body());
            if (is_unresolved_or_error(body)) return body;
            auto next = type::unifying_conversion(*current, *body);
            if (is_unresolved_or_error(next)) {
                return raise({
                        code::inconsistent_type,
                        extract_region(alternative.body()),
                        std::move(body),
                        { type::category_of(*current) },
                });
            }
            current = std::move(next);
        }
        if (auto e = expr.default_expression()) {
            auto body = resolve(*e);
            if (is_unresolved_or_error(body)) return body;
            auto next = type::unifying_conversion(*current, *body);
            if (is_unresolved_or_error(next)) {
                return raise({
                        code::inconsistent_type,
                        extract_region(*e),
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
            if (is_unresolved_or_error(alt)) return alt;
            auto next = type::unifying_conversion(*current, *alt);
            if (is_unresolved_or_error(next)) {
                return raise({
                        code::inconsistent_type,
                        extract_region(alternative),
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
            resolve(decl.value());
            ana_.variables().bind(decl.variable(), decl.value(), true);
        }
        return resolve(expr.body());
    }

    type_ptr operator()(scalar::function_call const& expr) {
        if (validate_) {
            // FIXME: type resolution has been completed since overload was detected
            for (auto&& arg : expr.arguments()) {
                resolve(arg);
            }
        }
        auto&& func = binding::unwrap(expr.function());
        return func.declaration().shared_return_type();
    }

    // FIXME: more expressions

    type_ptr operator()(enum_tag_t<scalar::unary_operator::plus>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
                return raise({
                        code::ambiguous_type,
                        extract_region(expr.operand()),
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
                        extract_region(expr.operand()),
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
                        extract_region(expr.operand()),
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
                        extract_region(expr.operand()),
                        std::move(operand),
                        { category::number, category::time_interval },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::length>, scalar::unary const& expr, type_ptr operand) {
        if (validate_) {
            switch (type::category_of(*operand)) {
                case category::character_string:
                case category::bit_string:
                case category::unresolved:
                    break; // ok
                case category::unknown:
                    report({
                            code::ambiguous_type,
                            extract_region(expr.operand()),
                            std::move(operand),
                            { category::character_string, category::bit_string },
                    });
                    break;
                default:
                    report({
                            code::unsupported_type,
                            extract_region(expr.operand()),
                            std::move(operand),
                            { category::character_string, category::bit_string },
                    });
                    break;
            }
        }
        return repo_.get(::takatori::type::int4());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::conditional_not>, scalar::unary const& expr, type_ptr operand) {
        if (validate_) {
            switch (type::category_of(*operand)) {
                case category::unknown:
                case category::boolean:
                case category::unresolved:
                    break; // ok
                default:
                    report({
                            code::unsupported_type,
                            extract_region(expr.operand()),
                            std::move(operand),
                            { category::boolean },
                    });
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_null>, scalar::unary const& expr, type_ptr operand) {
        if (validate_) {
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
                            extract_region(expr.operand()),
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
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_true>, scalar::unary const& expr, type_ptr operand) {
        if (validate_) {
            switch (type::category_of(*operand)) {
                case category::unknown:
                case category::boolean:
                case category::unresolved:
                    break; // ok
                default:
                    report({
                            code::unsupported_type,
                            extract_region(expr.operand()),
                            std::move(operand),
                            { category::boolean },
                    });
                    break;
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_false>, scalar::unary const& expr, type_ptr operand) {
        return operator()(enum_tag<scalar::unary_operator::is_true>, expr, std::move(operand));
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
                        extract_region(expr.left()),
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
                        auto&& ld = unsafe_downcast<::takatori::type::decimal>(*left);
                        auto&& rd = unsafe_downcast<::takatori::type::decimal>(*right);
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(max(p-s, q-t) + max(s,t) + 1, max(s,t))
                        return repo_.get(::takatori::type::decimal(
                                max(ld.precision() - ld.scale(), rd.precision() - rd.scale()) + std::max(ld.scale(), rd.scale()) + 1,
                                std::max(ld.scale(), rd.scale())));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::number },
                });
            case category::temporal:
                if (rcat == category::time_interval) return type::unary_temporal_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::time_interval },
                });
            case category::time_interval:
                if (rcat == category::temporal) return type::unary_temporal_promotion(*right, repo_);
                if (rcat == category::time_interval) return type::binary_time_interval_promotion(*left, *right, repo_);
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::temporal, category::time_interval },
                });
            default:
                return raise({
                        code::unsupported_type,
                        extract_region(expr.left()),
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
                        extract_region(expr.left()),
                        std::move(left),
                        { category::number, category::temporal, category::time_interval },
                });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        auto&& ld = unsafe_downcast<::takatori::type::decimal>(*left);
                        auto&& rd = unsafe_downcast<::takatori::type::decimal>(*right);
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(max(p-s, q-t) + max(s,t) + 1, max(s,t))
                        return repo_.get(::takatori::type::decimal(
                                max(ld.precision() - ld.scale(), rd.precision() - rd.scale()) + std::max(ld.scale(), rd.scale()) + 1,
                                std::max(ld.scale(), rd.scale())));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::number },
                });
            case category::temporal:
                if (rcat == category::time_interval) return type::unary_temporal_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::time_interval },
                });
            case category::time_interval:
                // NOTE: <time_interval> - <temporal> is not defined
                if (rcat == category::time_interval) return type::binary_time_interval_promotion(*left, *right, repo_);
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::time_interval },
                });
            default:
                return raise({
                        code::unsupported_type,
                        extract_region(expr.left()),
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
                        extract_region(expr.left()),
                        std::move(left),
                        { category::number, category::time_interval },
                });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        auto&& ld = unsafe_downcast<::takatori::type::decimal>(*left);
                        auto&& rd = unsafe_downcast<::takatori::type::decimal>(*right);
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
                        extract_region(expr.right()),
                        std::move(right),
                        { category::number, category::time_interval },
                });
            case category::time_interval:
                if (rcat == category::number) return type::unary_time_interval_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::time_interval },
                });
            default:
                return raise({
                        code::unsupported_type,
                        extract_region(expr.left()),
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
                        extract_region(expr.left()),
                        std::move(left),
                        { category::number, category::time_interval },
                });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        auto&& ld = unsafe_downcast<::takatori::type::decimal>(*left);
                        auto&& rd = unsafe_downcast<::takatori::type::decimal>(*right);
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(p + q, s + t)
                        return repo_.get(::takatori::type::decimal(
                                ld.precision() + rd.precision(),
                                ld.scale() + rd.scale()));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::number },
                });
            case category::time_interval:
                if (rcat == category::number) return type::unary_time_interval_promotion(*left, repo_);
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::number },
                });
            default:
                return raise({
                        code::unsupported_type,
                        extract_region(expr.left()),
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
                        extract_region(expr.left()),
                        std::move(left),
                        { category::character_string, category::bit_string },
                });
            case category::character_string:
                if (rcat == category::character_string) {
                    auto result = type::binary_character_string_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::character
                            && right->kind() == ::takatori::type::type_kind::character) {
                        auto&& lv = unsafe_downcast<::takatori::type::character>(*left);
                        auto&& rv = unsafe_downcast<::takatori::type::character>(*right);
                        auto&& ret = unsafe_downcast<::takatori::type::character>(*result);
                        return repo_.get(::takatori::type::character(
                                ::takatori::type::varying_t(ret.varying()),
                                lv.length() + rv.length()));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::character_string },
                });
            case category::bit_string:
                if (rcat == category::bit_string) {
                    auto result = type::binary_bit_string_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::bit
                            && right->kind() == ::takatori::type::type_kind::bit) {
                        auto&& lv = unsafe_downcast<::takatori::type::bit>(*left);
                        auto&& rv = unsafe_downcast<::takatori::type::bit>(*right);
                        auto&& ret = unsafe_downcast<::takatori::type::bit>(*result);
                        return repo_.get(::takatori::type::bit(
                                ::takatori::type::varying_t(ret.varying()),
                                lv.length() + rv.length()));
                    }
                    return result;
                }
                return raise({
                        code::inconsistent_type,
                        extract_region(expr.right()),
                        std::move(right),
                        { category::bit_string },
                });
            case category::collection:
                // FIXME: impl array like
            default:
                return raise({
                        code::unsupported_type,
                        extract_region(expr.left()),
                        std::move(left),
                        { category::character_string, category::bit_string },
                });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::conditional_and>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        if (validate_) {
            auto [lcat, rcat] = category_pair(left, right);
            if (lcat != category::unresolved && rcat != category::unresolved) {
                switch (lcat) {
                    case category::unknown:
                    case category::boolean:
                        if (rcat != category::unknown && rcat != category::boolean) {
                            report({
                                    code::inconsistent_type,
                                    extract_region(expr.right()),
                                    std::move(right),
                                    { category::boolean },
                            });
                        }
                        break;
                    default:
                        report({
                                code::unsupported_type,
                                extract_region(expr.left()),
                                std::move(left),
                                { category::boolean },
                        });
                        break;
                }
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::conditional_or>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        return operator()(enum_tag<scalar::binary_operator::conditional_and>, expr, std::move(left), std::move(right));
    }

    // relational operators
    bool operator()(relation::find const& expr) {
        if (!resolve_read_like(expr)) {
            return false;
        }
        if (validate_) {
            if (!validate_keys(expr, expr.keys())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::scan const& expr) {
        if (!resolve_read_like(expr)) {
            return false;
        }
        if (validate_) {
            if (!validate_keys(expr, expr.upper().keys())) {
                return false;
            }
            if (!validate_keys(expr, expr.lower().keys())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::intermediate::join const& expr) {
        if (validate_) {
            if (!validate_condition(expr.condition())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::join_find const& expr) {
        if (!resolve_read_like(expr)) {
            return false;
        }
        if (validate_) {
            if (!validate_keys(expr, expr.keys())) {
                return false;
            }
            if (!validate_condition(expr.condition())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::join_scan const& expr) {
        if (!resolve_read_like(expr)) {
            return false;
        }
        if (validate_) {
            if (!validate_keys(expr, expr.upper().keys())) {
                return false;
            }
            if (!validate_keys(expr, expr.lower().keys())) {
                return false;
            }
            if (!validate_condition(expr.condition())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::project const& expr) {
        for (auto&& column : expr.columns()) {
            auto source = resolve(column.value());
            if (is_error(*source)) {
                return false;
            }
            ana_.variables().bind(column.variable(), column.value(), true);
        }
        return true;
    }

    bool operator()(relation::filter const& expr) {
        if (validate_) {
            if (!validate_condition(expr.condition())) {
                return false;
            }
        }
        return true;
    }

    constexpr bool operator()(relation::buffer const&) noexcept {
        return true;
    }

    bool operator()(relation::intermediate::aggregate const& expr) {
        if (validate_) {
            if (!validate_group_keys(expr, expr.group_keys())) {
                return false;
            }
        }
        return resolve_aggregate_columns(expr.columns());
    }

    bool operator()(relation::intermediate::distinct const& expr) {
        if (validate_) {
            if (!validate_group_keys(expr, expr.group_keys())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::intermediate::limit const& expr) {
        if (validate_) {
            if (!validate_group_keys(expr, expr.group_keys())) {
                return false;
            }
            if (!validate_sort_keys(expr, expr.sort_keys())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::intermediate::union_ const& expr) {
        for (auto&& mapping : expr.mappings()) {
            if (mapping.left() && mapping.right()) {
                auto&& left_var = resolve_stream_column(*mapping.left());
                auto&& right_var = resolve_stream_column(*mapping.right());
                auto left = ana_.inspect(left_var);
                auto right = ana_.inspect(right_var);
                if (is_unresolved_or_error(left) || is_unresolved_or_error(right)) {
                    return false;
                }
                auto t = type::unifying_conversion(*left, *right, repo_);
                if (is_error(*t)) {
                    report({
                            code::inconsistent_type,
                            extract_region(*mapping.right()),
                            std::move(right),
                            { type::category_of(*left) },
                    });
                    return false;
                }
                ana_.variables().bind(mapping.destination(), std::move(t), true);
            } else if (mapping.left()) {
                auto&& resolution = resolve_stream_column(*mapping.left());
                ana_.variables().bind(mapping.destination(), resolution, true);
            } else if (mapping.right()) {
                auto&& resolution = resolve_stream_column(*mapping.right());
                ana_.variables().bind(mapping.destination(), resolution, true);
            } else {
                // never come here
                std::abort();
            }
        }
        return true;
    }

    bool operator()(relation::intermediate::intersection const& expr) {
        if (validate_) {
            for (auto&& key_pair : expr.group_key_pairs()) {
                auto&& left_var = resolve_stream_column(key_pair.left());
                auto&& right_var = resolve_stream_column(key_pair.right());
                auto left = ana_.inspect(left_var);
                auto right = ana_.inspect(right_var);
                if (is_unresolved_or_error(left) || is_unresolved_or_error(right)) {
                    return false;
                }
                auto t = type::unifying_conversion(*left, *right, repo_);
                if (is_error(*t)) {
                    report({
                            code::inconsistent_type,
                            extract_region(key_pair.right()),
                            std::move(right),
                            { type::category_of(*left) },
                    });
                    return false;
                }
            }
        }
        return true;
    }

    bool operator()(relation::intermediate::difference const& expr) {
        if (validate_) {
            for (auto&& key_pair : expr.group_key_pairs()) {
                auto&& left_var = resolve_stream_column(key_pair.left());
                auto&& right_var = resolve_stream_column(key_pair.right());
                auto left = ana_.inspect(left_var);
                auto right = ana_.inspect(right_var);
                if (is_unresolved_or_error(left) || is_unresolved_or_error(right)) {
                    return false;
                }
                auto t = type::unifying_conversion(*left, *right, repo_);
                if (is_error(*t)) {
                    report({
                            code::inconsistent_type,
                            extract_region(key_pair.right()),
                            std::move(right),
                            { type::category_of(*left) },
                    });
                    return false;
                }
            }
        }
        return true;
    }

    bool operator()(relation::emit const& expr) {
        if (validate_) {
            for (auto&& column : expr.columns()) {
                auto&& r = resolve_stream_column(column.source());
                if (is_unresolved_or_error(r)) {
                    return false;
                }
            }
        }
        return true;
    }

    bool operator()(relation::write const& expr) {
        if (validate_) {
            auto&& relation = binding::unwrap(expr.destination());
            if (relation.kind() != binding::relation_info::kind_type::index) {
                throw std::domain_error("must be index");
            }
            auto&& table = unsafe_downcast<binding::index_info>(relation)
                    .declaration()
                    .table();
            if (!validate_table_write(expr, table, expr.keys())) {
                return false;
            }
            if (!validate_table_write(expr, table, expr.columns())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::step::join const& expr) {
        if (validate_) {
            if (!validate_condition(expr.condition())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::step::aggregate const& expr) {
        return resolve_aggregate_columns(expr.columns());
    }

    bool operator()(relation::step::intersection const&) {
        return true;
    }

    bool operator()(relation::step::difference const&) {
        return true;
    }

    bool operator()(relation::step::flatten const&) {
        return true;
    }

    bool operator()(relation::step::take_flat const& expr) {
        return resolve_exchange_columns(expr, expr.columns());
    }

    bool operator()(relation::step::take_group const& expr) {
        return resolve_exchange_columns(expr, expr.columns());
    }

    bool operator()(relation::step::take_cogroup const& expr) {
        for (auto&& group : expr.groups()) {
            if (!resolve_exchange_columns(expr, group.columns())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::step::offer const& expr) {
        for (auto&& column : expr.columns()) {
            auto&& source = resolve_stream_column(column.source());
            if (is_unresolved_or_error(source)) {
                return false;
            }
            if (auto&& destination = ana_.variables().find(column.destination());
                    !destination) {
                // first time
                ana_.variables().bind(column.destination(), source, true);
            } else {
                // two or more (union)
                auto srct = ana_.inspect(source);
                auto dstt = ana_.inspect(destination);
                if (is_unresolved_or_error(srct) || is_unresolved_or_error(dstt)) {
                    return false;
                }
                auto t = type::unifying_conversion(*srct, *dstt, repo_);
                if (is_error(*t)) {
                    report({
                            code::inconsistent_type,
                            column.source().region(),
                            std::move(srct),
                            { type::category_of(*dstt) },
                    });
                }
            }
        }
        return true;
    }

    bool operator()(plan::process const& step) {
        auto&& g = step.operators();
        for (auto&& r : g) {
            if (!resolve(r, true)) {
                return false;
            }
        }
        return true;
    }

    bool operator()(plan::forward const& step) {
        if (validate_) {
            for (auto&& column : step.columns()) {
                resolve_exchange_column(column);
            }
        }
        return true;
    }

    bool operator()(plan::group const& step) {
        if (validate_) {
            for (auto&& column : step.columns()) {
                resolve_exchange_column(column);
            }
            for (auto&& column : step.group_keys()) {
                resolve_exchange_column(column);
            }
            for (auto&& key : step.sort_keys()) {
                resolve_exchange_column(key.variable());
            }
        }
        return true;
    }

    bool operator()(plan::aggregate const& step) {
        if (!resolve_aggregate_columns(step.aggregations())) {
            return false;
        }
        if (validate_) {
            for (auto&& column : step.source_columns()) {
                resolve_exchange_column(column);
            }
            for (auto&& column : step.group_keys()) {
                resolve_exchange_column(column);
            }
            for (auto&& column : step.destination_columns()) {
                resolve_exchange_column(column);
            }
        }
        return true;
    }

    bool operator()(plan::broadcast const& step) {
        if (validate_) {
            for (auto&& column : step.columns()) {
                resolve_exchange_column(column);
            }
        }
        return true;
    }

    bool operator()(plan::discard const&) {
        return true;
    }

private:
    expression_analyzer& ana_;
    std::vector<type_diagnostic, ::takatori::util::object_allocator<type_diagnostic>>& diagnostics_;
    type::repository& repo_;
    bool validate_;

    static ::takatori::document::region const& extract_region(scalar::expression const& expr) noexcept {
        return expr.region();
    }

    [[nodiscard]] ::takatori::document::region const& extract_region(::takatori::descriptor::variable const& variable) const noexcept {
        if (auto&& r = variable.region()) {
            return r;
        }
        if (auto r = ana_.variables().find(variable);
                r.kind() == variable_resolution::kind_type::scalar_expression) {
            return extract_region(r.element<variable_resolution::kind_type::scalar_expression>());
        }
        static ::takatori::document::region undefined;
        return undefined;
    }

    void report(type_diagnostic diagnostic) {
        diagnostics_.emplace_back(std::move(diagnostic));
    }

    type_ptr raise(type_diagnostic diagnostic) {
        report(std::move(diagnostic));
        static auto result = std::make_shared<type::extensions::error>();
        return result;
    }

    template<class Expr, class Keys>
    bool validate_keys(Expr const&, Keys const& keys) {
        for (auto&& key : keys) {
            auto&& varres = resolve_stream_column(key.variable());
            auto var = ana_.inspect(varres);
            auto val = resolve(key.value());
            if (is_unresolved_or_error(var) || is_unresolved_or_error(val)) {
                return false;
            }
            auto t = type::is_assignment_convertible(*val, *var);
            if (t != ternary::yes) {
                report({
                        code::inconsistent_type,
                        key.value().region(),
                        std::move(val),
                        { type::category_of(*var) },
                });
                return false;
            }
        }
        return true;
    }

    bool validate_condition(::takatori::util::optional_ptr<scalar::expression const> condition) {
        if (condition) {
            auto cond = resolve(*condition);
            switch (type::category_of(*cond)) {
                case category::unresolved: // NG but continue
                case category::unknown: // ok
                case category::boolean: // ok
                    break;
                default:
                    report({
                            code::inconsistent_type,
                            extract_region(*condition),
                            std::move(cond),
                            { category::boolean },
                    });
                    return false;
            }
        }
        return true;
    }

    template<class Expr, class Keys>
    bool validate_group_keys(Expr const&, Keys const& keys) {
        for (auto&& key : keys) {
            auto&& r = resolve_stream_column(key);
            if (is_unresolved_or_error(r)) {
                return false;
            }
        }
        return true;
    }

    template<class Expr, class Keys>
    bool validate_sort_keys(Expr const&, Keys const& keys) {
        for (auto&& key : keys) {
            auto&& r = resolve_stream_column(key.variable());
            if (is_unresolved_or_error(r)) {
                return false;
            }
        }
        return true;
    }

    template<class T, class Seq>
    bool validate_table_write(T const&, storage::table const& table, Seq const& mappings) {
        for (auto&& mapping : mappings) {
            auto&& src = resolve_stream_column(mapping.source());
            auto&& dst = resolve_table_column(mapping.destination());
            if (auto column = dst.template element_if<variable_resolution::kind_type::table_column>();
                    !column
                    || column->optional_owner().get() != std::addressof(table)) {
                throw std::domain_error("invalid table column");
            }

            auto srct = ana_.inspect(src);
            auto dstt = ana_.inspect(dst);
            if (is_unresolved_or_error(srct) || is_unresolved_or_error(dstt)) {
                return false;
            }
            auto r = type::is_assignment_convertible(*srct, *dstt);
            if (r == ternary::no) {
                report({
                        code::inconsistent_type,
                        extract_region(mapping.source()),
                        std::move(srct),
                        { type::category_of(*dstt) },
                });
                return false;
            }
            if (r == ternary::unknown) {
                return false;
            }
        }
        return true;
    }

    template<class ReadLike>
    bool resolve_read_like(ReadLike const& expr) {
        for (auto&& column : expr.columns()) {
            auto&& resolution = resolve_external_relation_column(column.source());
            if (is_unresolved_or_error(resolution)) {
                return false;
            }
            ana_.variables().bind(column.destination(), resolution, true);
        }
        return true;
    }

    template<class Expr, class Seq>
    bool resolve_exchange_columns(Expr const&, Seq const& columns) {
        for (auto&& column : columns) {
            auto&& resolution = resolve_exchange_column(column.source());
            if (is_unresolved_or_error(resolution)) {
                return false;
            }
            ana_.variables().bind(column.destination(), resolution, true);
        }
        return true;
    }

    variable_resolution const& resolve_table_column(::takatori::descriptor::variable const& variable) {
        auto&& info = binding::unwrap(variable);
        using kind = binding::variable_info_kind;
        if (info.kind() != kind::table_column) {
            throw std::domain_error("must be a table column");
        }
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        auto&& v = unsafe_downcast<binding::table_column_info>(info);
        return ana_.variables().bind(variable, v.column(), true);
    }

    variable_resolution const& resolve_exchange_column(::takatori::descriptor::variable const& variable) {
        auto&& info = binding::unwrap(variable);
        using kind = binding::variable_info_kind;
        if (info.kind() != kind::exchange_column) {
            throw std::domain_error("must be an exchange column");
        }
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        throw std::domain_error("FIXME: unresolved exchange column");
    }

    variable_resolution const& resolve_external_relation_column(::takatori::descriptor::variable const& variable) {
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        auto&& info = binding::unwrap(variable);
        using kind = binding::variable_info_kind;
        switch (info.kind()) {
            case kind::table_column: {
                auto&& v = unsafe_downcast<binding::table_column_info>(info);
                return ana_.variables().bind(variable, v.column(), true);
            }
            case kind::exchange_column: {
                // FIXME: unresolved exchange
                throw std::domain_error("FIXME: unresolved exchange column");
            }
            default:
                throw std::domain_error("invalid variable");
        }
    }

    variable_resolution const& resolve_stream_column(::takatori::descriptor::variable const& variable) {
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        auto&& info = binding::unwrap(variable);
        using kind = binding::variable_info_kind;
        switch (info.kind()) {
            case kind::stream_variable: {
                // FIXME: stream variable
                throw std::domain_error("FIXME: unresolved stream variable");
            }
            case kind::frame_variable: {
                // FIXME: frame variable
                throw std::domain_error("FIXME: unresolved frame variable");
            }
            case kind::external_variable: {
                auto&& v = unsafe_downcast<binding::external_variable_info>(info);
                return ana_.variables().bind(variable, v.declaration(), true);
            }
            default:
                throw std::domain_error("invalid variable");
        }
    }

    template<class Seq>
    bool resolve_aggregate_columns(Seq const& columns) {
        for (auto&& column : columns) {
            auto&& function = binding::unwrap(column.function());
            auto&& declaration = function.declaration();
            if (validate_) {
                if (!declaration
                        || declaration.parameter_types().size() != column.arguments().size()) {
                    return false;
                }
                std::size_t index = 0;
                for (auto&& arg : column.arguments()) {
                    auto&& resolution = resolve_stream_column(arg);
                    auto t = ana_.inspect(resolution);
                    if (is_error(*t)) {
                        return false;
                    }
                    if (type::is_assignment_convertible(*t, declaration.parameter_types()[index]) == ternary::no) {
                        report({
                                code::inconsistent_type,
                                extract_region(arg),
                                declaration.shared_parameter_types()[index],
                                { type::category_of(declaration.parameter_types()[index]) },
                        });
                        return false;
                    }
                    ++index;
                }
            }
            ana_.variables().bind(column.destination(), function.declaration(), true);
        }
        return true;
    }

    [[nodiscard]] bool is_unresolved_or_error(variable_resolution const& resolution) const {
        if (!resolution) {
            return true;
        }
        auto type = ana_.inspect(resolution);
        return is_unresolved_or_error(type);
    }

    static bool is_unresolved_or_error(std::shared_ptr<::takatori::type::data const> const& t) {
        if (!t) {
            return true;
        }
        return type::category_of(*t) == category::unresolved;
    }
};

} // namespace

expression_analyzer::expression_analyzer()
    : expression_analyzer(::takatori::util::object_creator {})
{}

expression_analyzer::expression_analyzer(::takatori::util::object_creator creator)
    : expression_analyzer(
        creator.create_shared<expression_mapping>(creator),
        creator.create_shared<variable_mapping>(creator))
{}

expression_analyzer::expression_analyzer(
        std::shared_ptr<expression_mapping> expressions,
        std::shared_ptr<variable_mapping> variables) noexcept
    : expressions_(std::move(expressions))
    , variables_(std::move(variables))
    , diagnostics_(expressions_->get_object_creator().template allocator<decltype(diagnostics_)::value_type>())
{}

expression_mapping& expression_analyzer::expressions() noexcept {
    return *expressions_;
}

expression_mapping const& expression_analyzer::expressions() const noexcept {
    return *expressions_;
}

std::shared_ptr<expression_mapping> expression_analyzer::shared_expressions() noexcept {
    return expressions_;
}

variable_mapping& expression_analyzer::variables() noexcept {
    return *variables_;
}

variable_mapping const& expression_analyzer::variables() const noexcept {
    return *variables_;
}

std::shared_ptr<variable_mapping> expression_analyzer::shared_variables() noexcept {
    return variables_;
}

std::shared_ptr<::takatori::type::data const> expression_analyzer::inspect(
        variable_resolution const& resolution) const {
    using kind = variable_resolution::kind_type;
    switch (resolution.kind()) {
        case kind::unresolved: {
            return {};
        }
        case kind::unknown: {
            return resolution.element<kind::unknown>();
        }
        case kind::scalar_expression: {
            auto&& expr = resolution.element<kind::scalar_expression>();
            return expressions_->find(expr);
        }
        case kind::table_column: {
            auto&& column = resolution.element<kind::table_column>();
            return column.shared_type();
        }
        case kind::external: {
            auto&& decl = resolution.element<kind::external>();
            return decl.shared_type();
        }
        case kind::function_call: {
            auto&& decl = resolution.element<kind::function_call>();
            return decl.shared_return_type();
        }
        case kind::aggregation: {
            auto&& decl = resolution.element<kind::aggregation>();
            return decl.shared_return_type();
        }
    }
    std::abort();
}

std::shared_ptr<::takatori::type::data const> expression_analyzer::inspect(
        ::takatori::descriptor::variable const& variable) const {
    return inspect(variables_->find(variable));
}

std::shared_ptr<::takatori::type::data const> expression_analyzer::resolve(
        ::takatori::scalar::expression const& expression,
        bool validate,
        type::repository& repo) {
    return engine { *this, validate, diagnostics_, repo }
        .resolve(expression);
}

bool expression_analyzer::resolve(
        ::takatori::relation::expression const& expression,
        bool validate,
        bool recursive,
        type::repository& repo) {
    return engine { *this, validate, diagnostics_, repo }
        .resolve(expression, recursive);
}

bool expression_analyzer::resolve(
        ::takatori::plan::step const& step,
        bool validate,
        bool recursive,
        type::repository& repo) {
    return engine { *this, validate, diagnostics_, repo }
            .resolve(step, recursive);
}

bool expression_analyzer::has_diagnostics() const noexcept {
    return !diagnostics_.empty();
}

::takatori::util::sequence_view<type_diagnostic const> expression_analyzer::diagnostics() const noexcept {
    return diagnostics_;
}

void expression_analyzer::clear_diagnostics() noexcept {
    diagnostics_.clear();
}

} // namespace yugawara::analyzer
