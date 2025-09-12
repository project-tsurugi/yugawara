#include <yugawara/analyzer/expression_analyzer.h>

#include <functional>
#include <utility>
#include <vector>

#include <tsl/hopscotch_set.h>

#include <takatori/scalar/dispatch.h>
#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/relation/step/dispatch.h>
#include <takatori/plan/dispatch.h>
#include <takatori/statement/dispatch.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/octet.h>
#include <takatori/type/bit.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>
#include <takatori/type/datetime_interval.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/type/category.h>
#include <yugawara/type/comparable.h>
#include <yugawara/type/conversion.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/scalar/aggregate_function_call.h>

#include <yugawara/binding/extract.h>

#include <yugawara/storage/table.h>

namespace yugawara::analyzer {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;
namespace statement = ::takatori::statement;

using diagnostic_type = expression_analyzer::diagnostic_type;
using code = diagnostic_type::code_type;

using ::takatori::util::enum_tag;
using ::takatori::util::enum_tag_t;
using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;
using ::yugawara::extension::type::is_error;
using ::yugawara::type::category;
using ::yugawara::util::ternary;

namespace {

constexpr std::optional<std::size_t> operator+(std::optional<std::size_t> a, std::optional<std::size_t> b) noexcept {
    if (!a || !b) return {};
    return *a + *b;
}

template<class T>
constexpr T& unwrap(std::reference_wrapper<T> wrapper) noexcept {
    return wrapper;
}

template<class T>
using saw_set = ::tsl::hopscotch_set<
        T const*,
        std::hash<T const*>,
        std::equal_to<>>;

// FIXME: impl constant conversions
class engine {
public:
    using type_ptr = std::shared_ptr<::takatori::type::data const>;

    explicit engine(
            expression_analyzer& ana,
            bool validate,
            bool allow_unresolved,
            std::vector<diagnostic_type>& diagnostics,
            type::repository& repo) noexcept
        : ana_(ana)
        , diagnostics_(diagnostics)
        , repo_(repo)
        , validate_(validate)
        , allow_unresolved_(allow_unresolved)
    {}

    type_ptr resolve(scalar::expression const& expression) {
        if (auto resolved = ana_.expressions().find(expression)) {
            return resolved.shared_type();
        }
        auto resolved = scalar::dispatch(*this, expression);
        ana_.expressions().bind(expression, resolved, true);
        return resolved;
    }

    bool resolve(relation::expression const& expression, bool recursive) {
        if (recursive) {
            saw_set<relation::expression> saw {};
            return resolve_recursive(expression, saw);
        }
        return resolve_flat(expression);
    }

    bool resolve_recursive(
            relation::expression const& expression,
            saw_set<relation::expression>& saw) {
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
                throw_exception(std::invalid_argument("incomplete relational expression"));
            }
        }
        saw.emplace(std::addressof(expression));
        return resolve_flat(expression);
    }

    bool resolve_flat(relation::expression const& expression) {
        if (relation::is_available_in_step_plan(expression.kind())) {
            return relation::step::dispatch(*this, expression);
        }
        BOOST_ASSERT(relation::is_available_in_intermediate_plan(expression.kind())); // NOLINT
        return relation::intermediate::dispatch(*this, expression);
    }

    bool resolve(relation::graph_type const& graph) {
        bool result = true;
        relation::sort_from_upstream(graph, [&](relation::expression const& v) {
            if (result) {
                result = resolve(v, false);
            }
        });
        return result;
    }

    bool resolve(plan::step const& step, bool recursive) {
        if (recursive) {
            saw_set<plan::step> saw {};
            if (step.kind() == plan::step_kind::process) {
                return resolve_recursive(unsafe_downcast<plan::process>(step), saw);
            }
            return resolve_recursive(unsafe_downcast<plan::exchange>(step), saw);
        }
        return plan::dispatch(*this, step);
    }

    bool resolve(plan::graph_type const& graph) {
        bool result = true;
        plan::sort_from_upstream(graph, [&](plan::step const& v) {
            if (result) {
                result = resolve(v, false);
            }
        });
        return result;
    }

    template<class Step>
    bool resolve_recursive(Step const& step, saw_set<plan::step> saw) {
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

    bool resolve(::takatori::statement::statement const& stmt) {
        return ::takatori::statement::dispatch(*this, stmt);
    }

    type_ptr operator()(scalar::expression const& expr) {
        throw_exception(std::domain_error(string_builder {}
                << expr.kind()
                << string_builder::to_string));
    }

    bool operator()(relation::expression const& expr) {
        throw_exception(std::domain_error(string_builder {}
                << expr.kind()
                << string_builder::to_string));
    }

    bool operator()(plan::step const& step) {
        throw_exception(std::domain_error(string_builder {}
                << step.kind()
                << string_builder::to_string));
    }

    bool operator()(::takatori::statement::statement const& stmt) {
        throw_exception(std::domain_error(string_builder {}
                << stmt.kind()
                << string_builder::to_string));
    }

    type_ptr operator()(scalar::immediate const& expr) {
        if (validate_) {
            if (!allow_unresolved_ && is_unresolved_or_error(expr.optional_type())) {
                return raise({
                        code::unsupported_type,
                        string_builder {}
                                << "immediate expression type is unsupported: "
                                << expr.optional_type()
                                << string_builder::to_string,
                        extract_region(expr),
                });
            }
            auto vtype = literal_type(expr.value(), expr.type());
            if (vtype) {
                auto r = type::is_assignment_convertible(*vtype, expr.type());
                if (r != ternary::yes) {
                    return raise(code::inconsistent_type,
                            expr.region(),
                            expr.type(),
                            { type::category_of(*vtype) });
                }
            }
        }
        return expr.shared_type();
    }

    type_ptr literal_type(::takatori::value::data const& value, ::takatori::type::data const& type) {
        using k = ::takatori::value::value_kind;
        switch (value.kind()) {
            case k::unknown:
                return repo_.get(::takatori::type::unknown {});
            case k::boolean:
                return repo_.get(::takatori::type::boolean {});
            case k::int4:
                return repo_.get(::takatori::type::int4 {});
            case k::int8:
                return repo_.get(::takatori::type::int8 {});
            case k::float4:
                return repo_.get(::takatori::type::float4 {});
            case k::float8:
                return repo_.get(::takatori::type::float8 {});
            case k::decimal:
                return repo_.get(::takatori::type::decimal {});
            case k::character:
                return repo_.get(::takatori::type::character { ::takatori::type::varying });
            case k::octet:
                return repo_.get(::takatori::type::octet { ::takatori::type::varying });
            case k::bit:
                return repo_.get(::takatori::type::bit { ::takatori::type::varying });
            case k::date:
                return repo_.get(::takatori::type::date {});
            case k::time_of_day:
                return repo_.get(::takatori::type::time_of_day { extract_time_zone(type) });
            case k::time_point:
                return repo_.get(::takatori::type::time_point { extract_time_zone(type) });
            case k::datetime_interval:
                return repo_.get(::takatori::type::datetime_interval {});

            case k::array:
            case k::record:
            case k::extension:
                // FIXME: support
                return {};
        }
        std::abort();
    }

    static ::takatori::type::with_time_zone_t extract_time_zone(::takatori::type::data const& type) {
        switch (type.kind()) {
            case ::takatori::type::time_of_day::tag:
                return ::takatori::type::with_time_zone_t { unsafe_downcast<::takatori::type::time_of_day>(type).with_time_zone() };
            case ::takatori::type::time_point::tag:
                return ::takatori::type::with_time_zone_t { unsafe_downcast<::takatori::type::time_point>(type).with_time_zone() };
            default:
                return ~::takatori::type::with_time_zone;
        }
    }

    type_ptr operator()(scalar::variable_reference const& expr) {
        if (auto&& resolution = resolve_stream_column(expr.variable())) {
            if (auto type = ana_.inspect(resolution)) {
                return type;
            }
        }
        return raise({
                code::unresolved_variable,
                extract_region(expr),
        });
    }

    type_ptr operator()(scalar::unary const& expr) {
        auto operand = resolve(expr.operand());
        return scalar::dispatch(*this, expr.operator_kind(), expr, std::move(operand));
    }

    type_ptr operator()(scalar::cast const& expr) {
        if (validate_) {
            if (!allow_unresolved_ && is_unresolved_or_error(expr.optional_type())) {
                return raise({
                        code::unsupported_type,
                        string_builder {}
                                << "cast target type is unsupported: "
                                << expr.optional_type()
                                << string_builder::to_string,
                        extract_region(expr),
                });
            }
            auto operand = resolve(expr.operand());
            if (type::is_cast_convertible(*operand, expr.type()) == ternary::no) {
                report(code::unsupported_type,
                        extract_region(expr),
                        *operand,
                        { type::category_of(expr.type()) });
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
                report(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { lcat });
            }
            if (!type::is_comparable(expr.operator_kind(), *unify)) {
                report({
                        code::unsupported_type,
                        string_builder {}
                                << "unsupported comparison for the type: "
                                << "operator=" << expr.operator_kind() << ", "
                                << "type=" << *unify
                                << string_builder::to_string,
                        extract_region(expr),
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
                        report(code::unsupported_type,
                                extract_region(unwrap(property)),
                                *ptype,
                                { category::character_string });
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
                        report(code::unsupported_type,
                                extract_region(alternative.condition()),
                                *condition,
                                { category::boolean });
                }
            }
            auto body = resolve(alternative.body());
            if (is_unresolved_or_error(body)) return body;
            auto next = type::unifying_conversion(*current, *body);
            if (is_unresolved_or_error(next)) {
                return raise(code::inconsistent_type,
                        extract_region(alternative.body()),
                        *body,
                        { type::category_of(*current) });
            }
            current = std::move(next);
        }
        if (auto e = expr.default_expression()) {
            auto body = resolve(*e);
            if (is_unresolved_or_error(body)) return body;
            auto next = type::unifying_conversion(*current, *body);
            if (is_unresolved_or_error(next)) {
                return raise(code::inconsistent_type,
                        extract_region(*e),
                        *body,
                        { type::category_of(*current) });
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
                return raise(code::inconsistent_type,
                        extract_region(alternative),
                        *alt,
                        { type::category_of(*current) });
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
        auto&& func = binding::extract(expr.function());
        if (validate_) {
            validate_function_call(expr, func);
        }
        return func.shared_return_type();
    }

    type_ptr operator()(scalar::extension const& expr) {
        using extension::scalar::extension_id;
        if (expr.extension_id() == extension_id::aggregate_function_call_id) {
            return operator()(unsafe_downcast<extension::scalar::aggregate_function_call>(expr));
        }
        throw_exception(std::domain_error(string_builder {}
                << "unknown scalar expression extension: "
                << expr.extension_id()
                << string_builder::to_string));
    }

    type_ptr operator()(extension::scalar::aggregate_function_call const& expr) {
        auto&& func = binding::extract(expr.function());
        if (validate_) {
            validate_function_call(expr, func);
        }
        return func.shared_return_type();
    }

    // FIXME: more expressions

    type_ptr operator()(enum_tag_t<scalar::unary_operator::plus>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
                return raise(code::ambiguous_type,
                        extract_region(expr.operand()),
                        *operand,
                        { category::number, category::datetime_interval });
            case category::number:
                return type::unary_numeric_promotion(*operand, repo_);
            case category::datetime_interval:
                return type::unary_time_interval_promotion(*operand, repo_);
            case category::unresolved:
                return operand;
            default:
                return raise(code::unsupported_type,
                        extract_region(expr.operand()),
                        *operand,
                        { category::number, category::datetime_interval });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::sign_inversion>, scalar::unary const& expr, type_ptr operand) {
        switch (type::category_of(*operand)) {
            case category::unknown:
                return raise(code::ambiguous_type,
                        extract_region(expr.operand()),
                        *operand,
                        { category::number, category::datetime_interval });
            case category::number:
                return type::unary_numeric_promotion(*operand, repo_);
            case category::datetime_interval:
                return type::unary_time_interval_promotion(*operand, repo_);
            case category::unresolved:
                return operand;
            default:
                return raise(code::unsupported_type,
                        extract_region(expr.operand()),
                        *operand,
                        { category::number, category::datetime_interval });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::length>, scalar::unary const& expr, type_ptr const& operand) {
        if (validate_) {
            switch (type::category_of(*operand)) {
                case category::character_string:
                case category::octet_string:
                case category::bit_string:
                case category::unresolved:
                    break; // ok
                case category::unknown:
                    report(code::ambiguous_type,
                            extract_region(expr.operand()),
                            *operand,
                            { category::character_string, category::bit_string });
                    break;
                default:
                    report(code::unsupported_type,
                            extract_region(expr.operand()),
                            *operand,
                            { category::character_string, category::bit_string });
                    break;
            }
        }
        return repo_.get(::takatori::type::int4());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::conditional_not>, scalar::unary const& expr, type_ptr const& operand) {
        if (validate_) {
            switch (type::category_of(*operand)) {
                case category::unknown:
                case category::boolean:
                case category::unresolved:
                    break; // ok
                default:
                    report(code::unsupported_type,
                            extract_region(expr.operand()),
                            *operand,
                            { category::boolean });
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_null>, scalar::unary const& expr, type_ptr const& operand) {
        if (validate_) {
            switch (type::category_of(*operand)) {
                case category::unknown:
                case category::boolean:
                case category::number:
                case category::character_string:
                case category::octet_string:
                case category::bit_string:
                case category::temporal:
                case category::datetime_interval:
                case category::unresolved:
                    break; // ok
                default:
                    report(code::unsupported_type,
                            extract_region(expr.operand()),
                            *operand,
                            {
                                    category::boolean,
                                    category::number,
                                    category::character_string,
                                    category::octet_string,
                                    category::bit_string,
                                    category::temporal,
                                    category::datetime_interval,
                            });
                    break;
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_true>, scalar::unary const& expr, type_ptr const& operand) {
        if (validate_) {
            switch (type::category_of(*operand)) {
                case category::unknown:
                case category::boolean:
                case category::unresolved:
                    break; // ok
                default:
                    report(code::unsupported_type,
                            extract_region(expr.operand()),
                            *operand,
                            { category::boolean });
                    break;
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_false>, scalar::unary const& expr, type_ptr const& operand) {
        return operator()(enum_tag<scalar::unary_operator::is_true>, expr, operand);
    }

    type_ptr operator()(enum_tag_t<scalar::unary_operator::is_unknown>, scalar::unary const& expr, type_ptr const& operand) {
        return operator()(enum_tag<scalar::unary_operator::is_true>, expr, operand);
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
                return raise(code::ambiguous_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::temporal, category::datetime_interval });
            case category::number:
                // FIXME: number * unknown
                // FIXME: as decimal
                // binary numeric -> if decimal -> compute prec/scale
                if (rcat == category::number) {
                    return process_numeric_additive_binary_operator(left, right);
                }
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::number });
            case category::temporal:
                if (rcat == category::datetime_interval) return type::unary_temporal_promotion(*left, repo_);
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::datetime_interval });
            case category::datetime_interval:
                if (rcat == category::temporal) return type::unary_temporal_promotion(*right, repo_);
                if (rcat == category::datetime_interval) return type::binary_time_interval_promotion(*left, *right, repo_);
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::temporal, category::datetime_interval });
            default:
                return raise(code::unsupported_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::temporal, category::datetime_interval });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::subtract>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                return raise(code::ambiguous_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::temporal, category::datetime_interval });
            case category::number:
                if (rcat == category::number) {
                    return process_numeric_additive_binary_operator(left, right);
                }
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::number });
            case category::temporal:
                if (rcat == category::datetime_interval) return type::unary_temporal_promotion(*left, repo_);
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::datetime_interval });
            case category::datetime_interval:
                // NOTE: <time_interval> - <temporal> is not defined
                if (rcat == category::datetime_interval) return type::binary_time_interval_promotion(*left, *right, repo_);
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::datetime_interval });
            default:
                return raise(code::unsupported_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::temporal, category::datetime_interval });
        }
    }

    type_ptr process_numeric_additive_binary_operator(type_ptr const& left, type_ptr const& right) {
        auto result = type::binary_numeric_promotion(*left, *right, repo_);
        if (result->kind() == ::takatori::type::type_kind::decimal) {
            auto left_dec = type::unary_decimal_promotion(*left);
            auto right_dec = type::unary_decimal_promotion(*right);
            if (left_dec->kind() == ::takatori::type::type_kind::decimal
                    && right_dec->kind() == ::takatori::type::type_kind::decimal) {
                auto&& ld = unsafe_downcast<::takatori::type::decimal>(*left_dec);
                auto&& rd = unsafe_downcast<::takatori::type::decimal>(*right_dec);
                // left:decimal(p, s) * right:decimal(q, t) => decimal(*, max(s,t))
                std::optional<::takatori::type::decimal::size_type> scale {};
                if (ld.scale() && rd.scale()) {
                    scale = std::max(*ld.scale(), *rd.scale());
                }
                return repo_.get(::takatori::type::decimal({}, scale));
            }
        }
        return result;
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::multiply>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                return raise(code::ambiguous_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::datetime_interval });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(*, *)
                        return repo_.get(::takatori::type::decimal({}, {}));
                    }
                    return result;
                }
                if (rcat == category::datetime_interval) return type::unary_time_interval_promotion(*right, repo_);
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::number, category::datetime_interval });
            case category::datetime_interval:
                if (rcat == category::number) return type::unary_time_interval_promotion(*left, repo_);
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::datetime_interval });
            default:
                return raise(code::unsupported_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::datetime_interval });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::divide>, scalar::binary const& expr, type_ptr left, type_ptr right) {
        auto [lcat, rcat] = category_pair(left, right);
        if (lcat == category::unresolved) return left;
        if (rcat == category::unresolved) return right;
        switch (lcat) {
            case category::unknown:
                return raise(code::ambiguous_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::datetime_interval });
            case category::number:
                if (rcat == category::number) {
                    auto result = type::binary_numeric_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::decimal
                        && right->kind() == ::takatori::type::type_kind::decimal) {
                        // left:decimal(p, s) * right:decimal(q, t) => decimal(*, *)
                        return repo_.get(::takatori::type::decimal({}, {}));
                    }
                    return result;
                }
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::number });
            case category::datetime_interval:
                if (rcat == category::number) return type::unary_time_interval_promotion(*left, repo_);
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::number });
            default:
                return raise(code::unsupported_type,
                        extract_region(expr.left()),
                        *left,
                        { category::number, category::datetime_interval });
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
                return raise(code::ambiguous_type,
                        extract_region(expr.left()),
                        *left,
                        { category::character_string, category::octet_string, category::bit_string });
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
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::character_string });
            case category::octet_string:
                if (rcat == category::octet_string) {
                    auto result = type::binary_octet_string_promotion(*left, *right, repo_);
                    if (left->kind() == ::takatori::type::type_kind::octet
                            && right->kind() == ::takatori::type::type_kind::octet) {
                        auto&& lv = unsafe_downcast<::takatori::type::octet>(*left);
                        auto&& rv = unsafe_downcast<::takatori::type::octet>(*right);
                        auto&& ret = unsafe_downcast<::takatori::type::octet>(*result);
                        return repo_.get(::takatori::type::octet(
                                ::takatori::type::varying_t(ret.varying()),
                                lv.length() + rv.length()));
                    }
                    return result;
                }
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::octet_string });
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
                return raise(code::inconsistent_type,
                        extract_region(expr.right()),
                        *right,
                        { category::bit_string });
            case category::collection:
                // FIXME: impl array like
            default:
                return raise(code::unsupported_type,
                        extract_region(expr.left()),
                        *left,
                        { category::character_string, category::octet_string, category::bit_string });
        }
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::conditional_and>, scalar::binary const& expr, type_ptr const& left, type_ptr const& right) {
        if (validate_) {
            auto [lcat, rcat] = category_pair(left, right);
            if (lcat != category::unresolved && rcat != category::unresolved) {
                switch (lcat) {
                    case category::unknown:
                    case category::boolean:
                        if (rcat != category::unknown && rcat != category::boolean) {
                            report(code::inconsistent_type,
                                    extract_region(expr.right()),
                                    *right,
                                    { category::boolean });
                        }
                        break;
                    default:
                        report(code::unsupported_type,
                                extract_region(expr.left()),
                                *left,
                                { category::boolean });
                        break;
                }
            }
        }
        return repo_.get(::takatori::type::boolean());
    }

    type_ptr operator()(enum_tag_t<scalar::binary_operator::conditional_or>, scalar::binary const& expr, type_ptr const& left, type_ptr const& right) {
        return operator()(enum_tag<scalar::binary_operator::conditional_and>, expr, left, right);
    }

    // relational operators
    bool operator()(relation::find const& expr) {
        if (!resolve_read_like(expr)) {
            return false;
        }
        if (validate_) {
            if (!validate_keys(expr, expr.keys(), false)) {
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
            if (!validate_keys(expr, expr.upper().keys(), true)) {
                return false;
            }
            if (!validate_keys(expr, expr.lower().keys(), true)) {
                return false;
            }
        }
        return true;
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    bool operator()(relation::values const& expr) {
        // first, we check number of columns whether validate is enabled
        for (auto&& row : expr.rows()) {
            if (expr.columns().size() < row.elements().size()) {
                report({
                        code::inconsistent_elements,
                        "too many values",
                        extract_region(row.elements()[expr.columns().size()]),
                });
                return false;
            }
            if (expr.columns().size() > row.elements().size()) {
                if (row.elements().empty()) {
                    report({
                            code::inconsistent_elements,
                            "too short values",
                            extract_region(expr.columns()[0]),
                    });
                } else {
                    report({
                            code::inconsistent_elements,
                            "too short values",
                            extract_region(row.elements().back()),
                    });
                }
                return false;
            }
        }

        // if no rows, we set columns type to unknown
        if (expr.rows().empty()) {
            for (auto&& c : expr.columns()) {
                ana_.variables().bind(c, { ::takatori::type::unknown(), repo_ }, true);
            }
            return true;
        }

        if (expr.rows().size() == 1) {
            for (std::size_t i = 0, n = expr.columns().size(); i < n; ++i) {
                auto&& column = expr.columns()[i];
                auto&& row = expr.rows()[0];
                auto&& value = row.elements()[i];
                auto source = resolve(value);
                if (is_unresolved_or_error(source)) {
                    return false;
                }
                ana_.variables().bind(column, value, true);
            }
            return true;
        }

        for (std::size_t i = 0, n = expr.columns().size(); i < n; ++i) {
            auto&& column = expr.columns()[i];
            std::shared_ptr<::takatori::type::data const> current;
            for (auto&& row : expr.rows()) {
                auto&& value = row.elements()[i];
                auto next = resolve(value);
                if (is_unresolved_or_error(next)) {
                    return false;
                }
                if (!current) {
                    current = std::move(next);
                } else {
                    auto unify = type::unifying_conversion(*current, *next);
                    auto ucat = type::category_of(*unify);
                    if (ucat == category::unresolved) {
                        report(code::inconsistent_type,
                                extract_region(value),
                                *next,
                                { type::category_of(*current) });
                        return false;
                    }
                    current = std::move(unify);
                }
            }
            BOOST_ASSERT(current); // NOLINT
            ana_.variables().bind(column, std::move(current), true);
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
            if (!validate_keys(expr, expr.keys(), false)) {
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
            if (!validate_keys(expr, expr.upper().keys(), true)) {
                return false;
            }
            if (!validate_keys(expr, expr.lower().keys(), true)) {
                return false;
            }
            if (!validate_condition(expr.condition())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::project const& expr) {
        for (auto&& column : expr.columns()) { // NOLINT(readability-use-anyofallof) misdetection?
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

    bool operator()(relation::identify const& expr) {
        ana_.variables().bind(expr.variable(), variable_resolution { expr.shared_type() });
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

    bool operator()(relation::intermediate::union_ const& expr) { // NOLINT(readability-function-cognitive-complexity)
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
                    report(code::inconsistent_type,
                            extract_region(*mapping.right()),
                            *right,
                            { type::category_of(*left) });
                    return false;
                }
                if (expr.quantifier() == relation::set_quantifier::distinct
                        && !validate_equality_comparable(*t, extract_region(*mapping.left()))) {
                    return false;
                }
                ana_.variables().bind(mapping.destination(), std::move(t), true);
            } else if (mapping.left()) {
                auto&& resolution = resolve_stream_column(*mapping.left());
                if (is_unresolved_or_error(resolution)) {
                    return false;
                }
                if (expr.quantifier() == relation::set_quantifier::distinct) {
                    auto t = ana_.inspect(resolution);
                    if (!validate_equality_comparable(*t, extract_region(*mapping.left()))) {
                        return false;
                    }
                }
                ana_.variables().bind(mapping.destination(), resolution, true);
            } else if (mapping.right()) {
                auto&& resolution = resolve_stream_column(*mapping.right());
                if (expr.quantifier() == relation::set_quantifier::distinct) {
                    auto t = ana_.inspect(resolution);
                    if (!validate_equality_comparable(*t, extract_region(*mapping.right()))) {
                        return false;
                    }
                }
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
            auto r = validate_key_pairs(expr, expr.group_key_pairs());
            if (!r) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::intermediate::difference const& expr) {
        if (validate_) {
            auto r = validate_key_pairs(expr, expr.group_key_pairs());
            if (!r) {
                return false;
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
            auto index = binding::extract<storage::index>(expr.destination());
            auto&& table = index.table();
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

    bool operator()(relation::step::take_cogroup const& expr) { // NOLINT(*-function-cognitive-complexity)
        std::optional<std::size_t> group_count {};
        for (auto&& group : expr.groups()) {
            auto exchange = binding::extract_if<plan::exchange>(group.source());
            if (!exchange || exchange->kind() != plan::step_kind::group) {
                report({
                        code::unknown,
                        "take_cogroup source must be a group exchange",
                        expr.region(),
                });
                return false;
            }
            auto&& group_exchange = unsafe_downcast<plan::group>(*exchange);
            if (!group_count) {
                group_count = group_exchange.group_keys().size();
            } else if (*group_count != group_exchange.group_keys().size()) {
                report({
                        code::inconsistent_elements,
                        "inconsistent number of group keys",
                        expr.region(),
                });
                return false;
            }
        }
        for (std::size_t pos = 0, size = *group_count; pos < size; ++pos) {
            bool promoted = false;
            type_ptr current {};
            for (auto&& group : expr.groups()) {
                auto&& exchange = binding::extract<plan::exchange>(group.source());
                auto&& group_exchange = unsafe_downcast<plan::group>(exchange);
                auto&& column = group_exchange.group_keys()[pos];
                auto source = ana_.inspect(column);
                if (is_unresolved_or_error(source)) {
                    return false;
                }
                if (!current) {
                    current = std::move(source);
                } else if (*current != *source) {
                    auto t = type::unifying_conversion(*current, *source, repo_);
                    if (is_error(*t)) {
                        report(code::inconsistent_type,
                                column.region(),
                                *source,
                                { type::category_of(*current) });
                        return false;
                    }
                    current = std::move(t);
                    promoted = true;
                    // NOTE: we don't need to check the equality comparability here,
                    //       because upstream grouping operation already required it.
                }
            }
            if (promoted) {
                for (auto&& group : expr.groups()) {
                    auto&& exchange = binding::extract<plan::exchange>(group.source());
                    auto&& group_exchange = unsafe_downcast<plan::group>(exchange);
                    auto&& column = group_exchange.group_keys()[pos];
                    ana_.variables().bind(column, current, true);
                }
            }
        }
        for (auto&& group : expr.groups()) { // NOLINT(readability-use-anyofallof) w/ side effects
            if (!resolve_exchange_columns(expr, group.columns())) {
                return false;
            }
        }
        return true;
    }

    bool operator()(relation::step::offer const& expr) {
        for (auto&& column : expr.columns()) { // NOLINT(readability-use-anyofallof) w/ side effects
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
                    report(code::inconsistent_type,
                            column.source().region(),
                            *srct,
                            { type::category_of(*dstt) });
                }
                ana_.variables().bind(column.destination(), std::move(t), true);
            }
        }
        return true;
    }

    bool operator()(plan::process const& step) {
        return resolve(step.operators());
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
                if (!validate_exchange_group_column(column)) {
                    return false;
                }
            }
            for (auto&& key : step.sort_keys()) {
                if (!validate_exchange_sort_column(key.variable())) {
                    return false;
                }
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
                if (!validate_exchange_group_column(column)) {
                    return false;
                }
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

    bool operator()(statement::execute const& stmt) {
        return resolve(stmt.execution_plan());
    }

    // NOLINTNEXTLINE(readability-function-cognitive-complexity)
    bool operator()(statement::write const& stmt) {
        if (validate_) {
            auto&& index = binding::extract<storage::index>(stmt.destination());
            auto&& table = index.table();
            for (auto&& tuple : stmt.tuples()) {
                if (stmt.columns().size() < tuple.elements().size()) {
                    report({
                            code::inconsistent_elements,
                            "too many values",
                            extract_region(tuple.elements()[stmt.columns().size()]),
                    });
                    return false;
                }
                if (stmt.columns().size() > tuple.elements().size()) {
                    if (tuple.elements().empty()) {
                        report({
                                code::inconsistent_elements,
                                "too short values",
                                extract_region(stmt.columns()[0]),
                        });
                    } else {
                        report({
                                code::inconsistent_elements,
                                "too short values",
                                extract_region(tuple.elements().back()),
                        });
                    }
                    return false;
                }
            }
            auto&& columns = stmt.columns();
            for (std::size_t i = 0, n = columns.size(); i < n; ++i) {
                auto&& dst = resolve_table_column(columns[i]);
                auto column = dst.element_if<variable_resolution::kind_type::table_column>();
                if (!column || column->optional_owner().get() != std::addressof(table)) {
                    throw_exception(std::domain_error("invalid table column"));
                }
                for (auto&& tuple : stmt.tuples()) {
                    auto&& src = resolve(tuple.elements()[i]);
                    if (!is_unresolved_or_error(src)) {
                        auto t = type::is_assignment_convertible(*src, column->type());
                        if (t != ternary::yes) {
                            report(code::inconsistent_type,
                                    tuple.elements()[i].region(),
                                    *src,
                                    { type::category_of(column->type()) });
                        }
                    }
                }
            }
        }
        return true;
    }

    bool operator()(statement::create_table const& stmt) { // NOLINT(*-function-cognitive-complexity)
        if (validate_) {
            bool success = true;
            auto&& table = binding::extract(stmt.definition());
            for (auto&& column : table.columns()) {
                auto&& value = column.default_value();
                if (value.kind() == storage::column_value_kind::immediate) {
                    auto&& v = *value.element<storage::column_value_kind::immediate>();
                    if (auto vtype = literal_type(v, column.type())) {
                        if (auto r = type::is_assignment_convertible(*vtype, column.type()); r != ternary::yes) {
                            report({
                                    code::inconsistent_type,
                                    string_builder {}
                                        << "column \"" << column.simple_name() << "\" "
                                        << "has inconsistent type for its default value"
                                        << string_builder::to_string,
                                    first_region(stmt.definition(), stmt),
                            });
                            success = false;
                        }
                    }
                } else if (value.kind() == storage::column_value_kind::sequence) {
                    using k = ::takatori::type::type_kind;
                    if (column.type().kind() != k::int4 && column.type().kind() != k::int8) {
                        report({
                                code::inconsistent_type,
                                string_builder {}
                                        << "column \"" << column.simple_name() << "\" "
                                        << "must be more than 32-bit int for storing sequence values."
                                        << string_builder::to_string,
                                first_region(stmt.definition(), stmt),
                        });
                        success = false;
                    }
                } else if (value.kind() == storage::column_value_kind::function) {
                    auto&& func = value.element<storage::column_value_kind::function>();
                    if (!func->parameter_types().empty()) {
                        report({
                                code::inconsistent_elements,
                                "function call for default value must not have any arguments",
                                first_region(stmt.definition(), stmt),
                        });
                        success = false;
                    }
                    if (auto r = type::is_assignment_convertible(func->return_type(), column.type());
                            r != ternary::yes) {
                        report({
                                code::inconsistent_type,
                                string_builder {}
                                        << "function \"" << func->name() << "\" "
                                        << "has inconsistent type (" << func->return_type() << ") "
                                        << "for the column \"" << column.simple_name() << "\" "
                                        << "(" << column.type() << ")"
                                        << string_builder::to_string,
                                first_region(stmt.definition(), stmt),
                        });
                        success = false;
                    }
                }
            }
            if (auto&& index_opt = binding::extract_if<storage::index>(stmt.primary_key())) {
                auto&& index = *index_opt;
                for (auto&& key : index.keys()) {
                    if (index.features().contains(storage::index_feature::scan)) {
                        if (!type::is_order_comparable(key.column().type())) {
                            report({
                                    code::unsupported_type,
                                    string_builder {}
                                            << "primary key must be order comparable: "
                                            << table.simple_name() << "." << key.column().simple_name()
                                            << "(" << key.column().type() << ")"
                                            << string_builder::to_string,
                                    stmt.primary_key().region(),
                            });
                            success = false;
                        }
                    } else if (index.features().contains(storage::index_feature::find)) {
                        if (!type::is_equality_comparable(key.column().type())) {
                            report({
                                    code::unsupported_type,
                                    string_builder {}
                                            << "primary key must be equality comparable: "
                                            << table.simple_name() << "." << key.column().simple_name()
                                            << "(" << key.column().type() << ")"
                                            << string_builder::to_string,
                                    stmt.primary_key().region(),
                            });
                            success = false;
                        }
                    }
                }
            }
            return success;
        }
        return true;
    }

    bool operator()(statement::drop_table const& stmt) {
        (void) stmt;
        return true;
    }

    bool operator()(statement::create_index const& stmt) {
        if (validate_) {
            bool success = true;
            if (auto&& index_opt = binding::extract_if<storage::index>(stmt.definition())) {
                auto&& index = *index_opt;
                for (auto&& key : index.keys()) {
                    if (index.features().contains(storage::index_feature::scan)) {
                        if (!type::is_order_comparable(key.column().type())) {
                            report({
                                    code::unsupported_type,
                                    string_builder {}
                                            << "index key must be order comparable: "
                                            << index.table().simple_name() << "." << key.column().simple_name()
                                            << "(" << key.column().type() << ")"
                                            << string_builder::to_string,
                                    stmt.definition().region(),
                            });
                            success = false;
                        }
                    } else if (index.features().contains(storage::index_feature::find)) {
                        if (!type::is_equality_comparable(key.column().type())) {
                            report({
                                    code::unsupported_type,
                                    string_builder {}
                                            << "index key must be equality comparable: "
                                            << index.table().simple_name() << "." << key.column().simple_name()
                                            << "(" << key.column().type() << ")"
                                            << string_builder::to_string,
                                    stmt.definition().region(),
                            });
                            success = false;
                        }
                    }
                }
            }
            return success;
        }
        return true;
    }

    bool operator()(statement::drop_index const& stmt) {
        (void) stmt;
        return true;
    }

    bool operator()(statement::grant_table const& stmt) {
        (void) stmt;
        return true;
    }

    bool operator()(statement::revoke_table const& stmt) {
        (void) stmt;
        return true;
    }

    bool operator()(statement::empty const& stmt) {
        (void) stmt;
        return true;
    }

private:
    expression_analyzer& ana_;
    std::vector<diagnostic_type>& diagnostics_;
    type::repository& repo_;
    bool validate_;
    bool allow_unresolved_;

    static ::takatori::document::region const& extract_region(scalar::expression const& expr) noexcept {
        return expr.region();
    }

    [[nodiscard]] ::takatori::document::region const& extract_region(descriptor::variable const& variable) const noexcept {
        if (auto&& r = variable.region()) {
            return r;
        }
        if (auto r = ana_.variables().find(variable);
                r.kind() == variable_resolution::kind_type::scalar_expression) {
            return extract_region(r.element<variable_resolution::kind_type::scalar_expression>());
        }
        static ::takatori::document::region const undefined;
        return undefined;
    }

    [[nodiscard]] static ::takatori::document::region first_region() noexcept {
        return {};
    }

    template<class T, class...Rest>
    [[nodiscard]] static ::takatori::document::region first_region(T const& first, Rest&&... rest) noexcept {
        if (first.region()) {
            return first.region();
        }
        return first_region(std::forward<Rest>(rest)...);
    }

    void report(diagnostic_type diagnostic) {
        diagnostics_.emplace_back(std::move(diagnostic));
    }

    void report(
            code c,
            ::takatori::document::region region,
            ::takatori::type::data const& actual,
            type::category_set expected) {
        diagnostics_.emplace_back(
                c,
                string_builder {}
                        << actual
                        << " (expected: " << expected << ")"
                        << string_builder::to_string,
                region);
    }

    [[nodiscard]] type_ptr raise(diagnostic_type diagnostic) {
        report(std::move(diagnostic));
        static auto result = std::make_shared<extension::type::error>();
        return result;
    }

    [[nodiscard]] type_ptr raise(
            code c,
            ::takatori::document::region region,
            ::takatori::type::data const& actual,
            type::category_set expected) {
        report(c, region, actual, expected);
        static auto result = std::make_shared<extension::type::error>();
        return result;
    }

    template<class Expr, class F>
    void validate_function_call(Expr const& expr, F const& func) {
        if (!allow_unresolved_ && is_unresolved_or_error(func.optional_return_type())) {
            report({
                    code::unsupported_type,
                    string_builder {}
                            << "function return type is unsupported: "
                            << func.optional_return_type()
                            << string_builder::to_string,
                    extract_region(expr),
            });
            return;
        }
        if (func.parameter_types().size() != expr.arguments().size()) {
            report({
                    code::inconsistent_elements,
                    "inconsistent number of function arguments",
                    extract_region(expr),
            });
            return;
        }
        for (std::size_t i = 0, n = expr.arguments().size(); i < n; ++i) {
            auto&& arg = expr.arguments()[i];
            auto&& param = func.parameter_types()[i];
            auto r = resolve(arg);
            if (!is_unresolved_or_error(r)) {
                auto t = type::is_assignment_convertible(*r, param);
                if (t != ternary::yes) {
                    report(
                            code::inconsistent_type,
                            arg.region(),
                            *r,
                            { type::category_of(param) });
                    return;
                }
            }
        }
    }

    template<class Expr, class Keys>
    [[nodiscard]] bool validate_keys(Expr const&, Keys const& keys, bool range) {
        auto keys_count = keys.size();
        std::size_t index = 0;
        for (auto&& key : keys) { // NOLINT(readability-use-anyofallof) w/ side effects
            auto&& varres = resolve_external_relation_column(key.variable());
            auto var = ana_.inspect(varres);
            auto val = resolve(key.value());
            if (is_unresolved_or_error(var) || is_unresolved_or_error(val)) {
                return false;
            }
            auto t = type::is_assignment_convertible(*val, *var);
            if (t != ternary::yes) {
                report(
                        code::inconsistent_type,
                        key.value().region(),
                        *val,
                        { type::category_of(*var) });
                return false;
            }
            if (!validate_equality_comparable(*val, key.value().region())) {
                return false;
            }
            if (range) {
                ++index;
                if (index == keys_count && !validate_order_comparable(*val, key.value().region())) {
                    return false;
                }
            }
        }
        return true;
    }

    [[nodiscard]] bool validate_condition(::takatori::util::optional_ptr<scalar::expression const> condition) {
        if (condition) {
            auto cond = resolve(*condition);
            switch (type::category_of(*cond)) {
                case category::unresolved: // NG but continue
                case category::unknown: // ok
                case category::boolean: // ok
                    break;
                default:
                    report(code::inconsistent_type,
                            extract_region(*condition),
                            *cond,
                            { category::boolean });
                    return false;
            }
        }
        return true;
    }

    template<class Expr, class Keys>
    [[nodiscard]] bool validate_group_keys(Expr const&, Keys const& keys) {
        for (auto&& key : keys) { // NOLINT(readability-use-anyofallof) w/ side effects
            auto&& r = resolve_stream_column(key);
            if (is_unresolved_or_error(r)) {
                return false;
            }
            auto t = ana_.inspect(r);
            if (!validate_equality_comparable(*t, key.region())) {
                return false;
            }
        }
        return true;
    }

    template<class Expr, class Keys>
    [[nodiscard]] bool validate_sort_keys(Expr const&, Keys const& keys) {
        for (auto&& key : keys) { // NOLINT(readability-use-anyofallof) w/ side effects
            auto&& r = resolve_stream_column(key.variable());
            if (is_unresolved_or_error(r)) {
                return false;
            }
            auto t = ana_.inspect(r);
            if (!validate_order_comparable(*t, key.variable().region())) {
                return false;
            }
        }
        return true;
    }
    template<class Expr, class Keys>
    [[nodiscard]] bool validate_key_pairs(Expr const&, Keys const& key_pairs) {
        for (auto&& key_pair : key_pairs) { // NOLINT(readability-use-anyofallof)
            auto&& left_var = resolve_stream_column(key_pair.left());
            auto&& right_var = resolve_stream_column(key_pair.right());
            auto left = ana_.inspect(left_var);
            auto right = ana_.inspect(right_var);
            if (is_unresolved_or_error(left) || is_unresolved_or_error(right)) {
                return false;
            }
            auto t = type::unifying_conversion(*left, *right, repo_);
            if (is_error(*t)) {
                report(code::inconsistent_type,
                        extract_region(key_pair.right()),
                        *right,
                        { type::category_of(*left) });
                return false;
            }
            if (!validate_equality_comparable(*t, key_pair.left().region())) {
                return false;
            }
        }
        return true;
    }

    template<class T, class Seq>
    [[nodiscard]] bool validate_table_write(T const&, storage::table const& table, Seq const& mappings) {
        for (auto&& mapping : mappings) {
            auto&& src = resolve_stream_column(mapping.source());
            auto&& dst = resolve_table_column(mapping.destination());
            if (auto column = dst.template element_if<variable_resolution::kind_type::table_column>();
                    !column
                    || column->optional_owner().get() != std::addressof(table)) {
                throw_exception(std::domain_error("invalid table column"));
            }

            auto srct = ana_.inspect(src);
            auto dstt = ana_.inspect(dst);
            if (is_unresolved_or_error(srct) || is_unresolved_or_error(dstt)) {
                return false;
            }
            auto r = type::is_assignment_convertible(*srct, *dstt);
            if (r == ternary::no) {
                report(code::inconsistent_type,
                        extract_region(mapping.source()),
                       *srct,
                        { type::category_of(*dstt) });
                return false;
            }
            if (r == ternary::unknown) {
                return false;
            }
        }
        return true;
    }

    template<class ReadLike>
    [[nodiscard]] bool resolve_read_like(ReadLike const& expr) {
        for (auto&& column : expr.columns()) { // NOLINT(readability-use-anyofallof) w/ side effects
            auto&& resolution = resolve_external_relation_column(column.source());
            if (is_unresolved_or_error(resolution)) {
                return false;
            }
            ana_.variables().bind(column.destination(), resolution, true);
        }
        return true;
    }

    template<class Expr, class Seq>
    [[nodiscard]] bool resolve_exchange_columns(Expr const&, Seq const& columns) {
        for (auto&& column : columns) { // NOLINT(readability-use-anyofallof) w/ side effects
            auto&& resolution = resolve_exchange_column(column.source());
            if (is_unresolved_or_error(resolution)) {
                return false;
            }
            ana_.variables().bind(column.destination(), resolution, true);
        }
        return true;
    }

    variable_resolution const& resolve_table_column(descriptor::variable const& variable) {
        auto&& column = binding::extract<storage::column>(variable);
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        return ana_.variables().bind(variable, column, true);
    }

    variable_resolution const& resolve_exchange_column(descriptor::variable const& variable) {
        using kind = binding::variable_info_kind;
        if (binding::kind_of(variable) != kind::exchange_column) {
            throw_exception(std::domain_error("must be an exchange column"));
        }
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        if (!allow_unresolved_) {
            report({
                    code::unresolved_variable,
                    string_builder {}
                            << "exchange column is not yet resolved: " << variable
                            << string_builder::to_string,
                    variable.region(),
            });
        }
        return unresolved();
    }

    bool validate_exchange_group_column(descriptor::variable const& variable) {
        auto&& r = resolve_exchange_column(variable);
        if (!r.is_resolved()) {
            return allow_unresolved_;
        }
        auto t = ana_.inspect(r);
        if (is_error(*t)) {
            return false;
        }
        if (!validate_equality_comparable(*t, variable.region())) {
            return false;
        }
        return true;
    }

    bool validate_exchange_sort_column(descriptor::variable const& variable) {
        auto&& r = resolve_exchange_column(variable);
        if (!r.is_resolved()) {
            return allow_unresolved_;
        }
        auto t = ana_.inspect(r);
        if (is_error(*t)) {
            return false;
        }
        if (!validate_order_comparable(*t, variable.region())) {
            return false;
        }
        return true;
    }

    variable_resolution const& resolve_external_relation_column(descriptor::variable const& variable) {
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        using kind = binding::variable_info_kind;
        switch (binding::kind_of(variable)) {
            case kind::table_column: {
                auto&& column = binding::extract<storage::column>(variable);
                if (!allow_unresolved_ && is_unresolved_or_error(column.optional_type())) {
                    report({
                            code::unsupported_type,
                            string_builder {}
                                    << "unsupported column type: "
                                    << column.owner().simple_name() << "." << column.simple_name()
                                    << " (" << column.optional_type() << ")"
                                    << string_builder::to_string,
                            variable.region(),
                    });
                }
                return ana_.variables().bind(variable, column, true);
            }
            case kind::exchange_column:
                if (!allow_unresolved_) {
                    report({
                            code::unresolved_variable,
                            string_builder {}
                                    << "exchange column is not yet resolved: " << variable
                                    << string_builder::to_string,
                            variable.region(),
                    });
                }
                return unresolved();
            default:
                throw_exception(std::domain_error("invalid variable"));
        }
    }

    variable_resolution const& resolve_stream_column(descriptor::variable const& variable) {
        if (auto&& resolution = ana_.variables().find(variable)) {
            return resolution;
        }
        using kind = binding::variable_info_kind;
        switch (binding::kind_of(variable)) {
            case kind::frame_variable:
            case kind::stream_variable:
            case kind::local_variable: // NOTE: local variables can appear as stream variable
                if (!allow_unresolved_) {
                    report({
                            code::unresolved_variable,
                            string_builder {}
                                    << "variable is not yet resolved: " << variable
                                    << string_builder::to_string,
                            variable.region(),
                    });
                }
                return unresolved();

            case kind::external_variable: {
                auto&& decl = binding::extract<variable::declaration>(variable);
                if (!allow_unresolved_ && is_unresolved_or_error(decl.optional_type())) {
                    report({
                            code::unsupported_type,
                            string_builder{}
                                    << "unsupported external variable type: "
                                    << decl.name()
                                    << " (" << decl.optional_type() << ")"
                                    << string_builder::to_string,
                            variable.region(),
                    });
                    return unresolved();
                }
                return ana_.variables().bind(variable, decl, true);
            }

            default:
                throw_exception(std::domain_error("invalid variable"));
        }
    }

    template<class Seq>
    bool resolve_aggregate_columns(Seq const& columns) {
        for (auto&& column : columns) {
            auto&& function = binding::extract(column.function());
            if (validate_) {
                if (!function
                        || function.parameter_types().size() != column.arguments().size()) {
                    return false;
                }
                std::size_t index = 0;
                for (auto&& arg : column.arguments()) {
                    auto&& resolution = resolve_stream_column(arg);
                    auto t = ana_.inspect(resolution);
                    if (is_error(*t)) {
                        return false;
                    }
                    if (type::is_assignment_convertible(*t, function.parameter_types()[index]) == ternary::no) {
                        report(code::inconsistent_type,
                                extract_region(arg),
                                function.parameter_types()[index],
                                { type::category_of(function.parameter_types()[index]) });
                        return false;
                    }
                    ++index;
                }
            }
            ana_.variables().bind(column.destination(), function, true);
        }
        return true;
    }

    [[nodiscard]] bool validate_equality_comparable(
            ::takatori::type::data const& type,
            ::takatori::document::region const& region) {
        if (!type::is_equality_comparable(type)) {
            report({
                    code::unsupported_type,
                    string_builder {}
                            << "unsupported equality comparison for the type: "
                            << type
                            << string_builder::to_string,
                    region,
            });
            return false;
        }
        return true;
    }

    [[nodiscard]] bool validate_order_comparable(
            ::takatori::type::data const& type,
            ::takatori::document::region const& region) {
        if (!type::is_order_comparable(type)) {
            report({
                    code::unsupported_type,
                    string_builder {}
                            << "unsupported order comparison for the type: "
                            << type
                            << string_builder::to_string,
                    region,
            });
            return false;
        }
        return true;
    }

    [[nodiscard]] static variable_resolution const& unresolved() {
        static variable_resolution const unresolved;
        return unresolved;
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

    static bool is_unresolved_or_error(optional_ptr<::takatori::type::data const> t) {
        if (!t) {
            return true;
        }
        return type::category_of(*t) == category::unresolved;
    }
};

} // namespace

expression_analyzer::expression_analyzer()
    : expression_analyzer {
        std::make_shared<expression_mapping>(),
        std::make_shared<variable_mapping>(),
    }
{}

expression_analyzer::expression_analyzer(
        ::takatori::util::maybe_shared_ptr<expression_mapping> expressions,
        ::takatori::util::maybe_shared_ptr<variable_mapping> variables) noexcept
    : expressions_(std::move(expressions))
    , variables_(std::move(variables))
{}

expression_mapping& expression_analyzer::expressions() noexcept {
    return *expressions_;
}

expression_mapping const& expression_analyzer::expressions() const noexcept {
    return *expressions_;
}

::takatori::util::maybe_shared_ptr<expression_mapping> expression_analyzer::shared_expressions() noexcept {
    return expressions_;
}

variable_mapping& expression_analyzer::variables() noexcept {
    return *variables_;
}

variable_mapping const& expression_analyzer::variables() const noexcept {
    return *variables_;
}

::takatori::util::maybe_shared_ptr<variable_mapping> expression_analyzer::shared_variables() noexcept {
    return variables_;
}

bool expression_analyzer::allow_unresolved() const noexcept {
    return allow_unresolved_;
}

expression_analyzer& expression_analyzer::allow_unresolved(bool allow) noexcept {
    allow_unresolved_ = allow;
    return *this;
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
            return expressions_->find(expr).shared_type();
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
        descriptor::variable const& variable) const {
    return inspect(variables_->find(variable));
}

std::shared_ptr<::takatori::type::data const> expression_analyzer::resolve(
        scalar::expression const& expression,
        bool validate,
        type::repository& repo) {
    return engine { *this, validate, allow_unresolved_, diagnostics_, repo }
        .resolve(expression);
}

bool expression_analyzer::resolve(
        relation::expression const& expression,
        bool validate,
        bool recursive,
        type::repository& repo) {
    return engine { *this, validate, allow_unresolved_, diagnostics_, repo }
        .resolve(expression, recursive);
}

bool expression_analyzer::resolve(
        relation::graph_type const& graph,
        bool validate,
        type::repository& repo) {
    return engine { *this, validate, allow_unresolved_, diagnostics_, repo }
            .resolve(graph);
}

bool expression_analyzer::resolve(
        plan::step const& step,
        bool validate,
        bool recursive,
        type::repository& repo) {
    return engine { *this, validate, allow_unresolved_, diagnostics_, repo }
            .resolve(step, recursive);
}

bool expression_analyzer::resolve(
        plan::graph_type const& graph,
        bool validate,
        type::repository& repo) {
    return engine { *this, validate, allow_unresolved_, diagnostics_, repo }
            .resolve(graph);
}

bool expression_analyzer::resolve(
        statement::statement const& statement,
        bool validate,
        type::repository& repo) {
    return engine { *this, validate, allow_unresolved_, diagnostics_, repo }
            .resolve(statement);
}

bool expression_analyzer::has_diagnostics() const noexcept {
    return !diagnostics_.empty();
}

::takatori::util::sequence_view<diagnostic_type const> expression_analyzer::diagnostics() const noexcept {
    return diagnostics_;
}

void expression_analyzer::clear_diagnostics() noexcept {
    diagnostics_.clear();
}

} // namespace yugawara::analyzer
