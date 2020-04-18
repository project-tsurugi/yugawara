#include "simplify_predicate.h"

#include <takatori/type/primitive.h>

#include <takatori/value/primitive.h>

#include <takatori/scalar/dispatch.h>

#include <takatori/util/downcast.h>
#include <takatori/util/enum_tag.h>

namespace yugawara::analyzer::details {

namespace value = ::takatori::value;
namespace scalar = ::takatori::scalar;

using scalar::unary_operator;
using scalar::binary_operator;

using ::takatori::util::object_ownership_reference;
using ::takatori::util::unsafe_downcast;
using ::takatori::util::enum_tag_t;

using expression_ref = object_ownership_reference<scalar::expression>;

namespace {

constexpr auto constant_true = simplify_predicate_result::constant_true;
constexpr auto constant_false = simplify_predicate_result::constant_false;
constexpr auto constant_unknown = simplify_predicate_result::constant_unknown;
constexpr auto constant_true_or_unknown = simplify_predicate_result::constant_true_or_unknown;
constexpr auto constant_false_or_unknown = simplify_predicate_result::constant_false_or_unknown;
constexpr auto not_sure = simplify_predicate_result::not_sure;

constexpr simplify_predicate_result from_bool(bool value) noexcept {
    return value ? simplify_predicate_result::constant_true : simplify_predicate_result::constant_false;
}

class engine {
public:
    simplify_predicate_result dispatch(expression_ref& ref) {
        return scalar::dispatch(*this, *ref, ref);
    }

    simplify_predicate_result dispatch(expression_ref&& ref) {
        return scalar::dispatch(*this, *ref, ref);
    }

    constexpr simplify_predicate_result operator()(scalar::expression const&, expression_ref const&) noexcept {
        return not_sure;
    }

    simplify_predicate_result operator()(scalar::immediate const& expr, expression_ref const&) {
        switch (expr.value().kind()) {
            case value::boolean::tag:
                return from_bool(unsafe_downcast<value::boolean>(expr.value()).get());
            case value::unknown::tag:
                return constant_unknown;
            default:
                return not_sure;
        }
    }

    simplify_predicate_result operator()(scalar::unary& expr, expression_ref& ref) {
        return scalar::dispatch(*this, expr.operator_kind(), expr, ref);
    }

    constexpr simplify_predicate_result operator()(unary_operator, scalar::unary const&, expression_ref const&) noexcept {
        return not_sure;
    }

    simplify_predicate_result operator()(enum_tag_t<unary_operator::conditional_not>, scalar::unary& expr, expression_ref&) {
        auto r = dispatch(expr.ownership_operand());
        switch (r) {
            case constant_true: return constant_false;
            case constant_false: return constant_true;
            case constant_unknown: return constant_unknown;
            case constant_true_or_unknown: return constant_false_or_unknown;
            case constant_false_or_unknown: return constant_true_or_unknown;
            case not_sure: return not_sure;
        }
        std::abort();
    }

    simplify_predicate_result operator()(enum_tag_t<unary_operator::is_null>, scalar::unary& expr, expression_ref&) {
        auto r = dispatch(expr.ownership_operand());
        switch (r) {
            case constant_unknown:
                return constant_true;
            case constant_true:
            case constant_false:
                return constant_false;
            case constant_true_or_unknown:
            case constant_false_or_unknown:
            case not_sure:
                return not_sure;
        }
        std::abort();
    }

    simplify_predicate_result operator()(enum_tag_t<unary_operator::is_true>, scalar::unary& expr, expression_ref&) {
        auto r = dispatch(expr.ownership_operand());
        switch (r) {
            case constant_true:
                return constant_true;
            case constant_false:
            case constant_unknown:
            case constant_false_or_unknown:
                return constant_false;
            case constant_true_or_unknown:
            case not_sure:
                return not_sure;
        }
        std::abort();
    }

    simplify_predicate_result operator()(enum_tag_t<unary_operator::is_false>, scalar::unary& expr, expression_ref&) {
        auto r = dispatch(expr.ownership_operand());
        switch (r) {
            case constant_false:
                return constant_true;
            case constant_true:
            case constant_unknown:
            case constant_true_or_unknown:
                return constant_false;
            case constant_false_or_unknown:
            case not_sure:
                return not_sure;
        }
        std::abort();
    }

    simplify_predicate_result operator()(enum_tag_t<unary_operator::is_unknown>, scalar::unary& expr, expression_ref&) {
        auto r = dispatch(expr.ownership_operand());
        switch (r) {
            case constant_unknown:
                return constant_true;
            case constant_true:
            case constant_false:
                return constant_false;
            case constant_true_or_unknown:
            case constant_false_or_unknown:
            case not_sure:
                return not_sure;
        }
        std::abort();
    }

    simplify_predicate_result operator()(scalar::binary& expr, expression_ref& ref) {
        using kind = scalar::binary_operator;
        switch (expr.operator_kind()) {
            case kind::conditional_and:
                return process_conditional_and(expr, ref);
            case kind::conditional_or:
                return process_conditional_or(expr, ref);
            default:
                return not_sure;
        }
    }

    simplify_predicate_result process_conditional_and(scalar::binary& expr, expression_ref& ref) {
        // first, process as two-valued logic
        auto left = dispatch(expr.ownership_left());
        switch (left) {
            case constant_true:
                ref = expr.release_right();
                return dispatch(ref);

            case constant_false:
                return constant_false;

            default:
                break;
        }
        auto right = dispatch(expr.ownership_right());
        switch (right) {
            case constant_true:
                ref = expr.release_left();
                return left;

            case constant_false:
                return constant_false;

            default:
                break;
        }

        /*
         * | && | T? | F? | U  | X  |
         * +----+----+----+----+----+
         * | T? | T? | F? | U  | X  | -> = right
         * | F? | F? | F? | F? | F? | -> = F?
         * | U  | U  | F? | U  | F? |
         * | X  | X  | F? | F? | X  |
         */
        switch (left) {
            case constant_true_or_unknown:
                return right;

            case constant_false_or_unknown:
                return constant_false_or_unknown;

            case constant_unknown:
                switch (right) {
                    case constant_true_or_unknown:
                    case constant_unknown:
                        return constant_unknown;
                    case constant_false_or_unknown:
                    case not_sure:
                        return constant_false_or_unknown;
                    default:
                        std::abort();
                }

            case not_sure:
                switch (right) {
                    case constant_unknown:
                    case constant_false_or_unknown:
                        return constant_false_or_unknown;
                    case constant_true_or_unknown:
                    case not_sure:
                        return not_sure;
                    default:
                        std::abort();
                }

            default: std::abort();
        }
    }

    simplify_predicate_result process_conditional_or(scalar::binary& expr, expression_ref& ref) {
        // first, process as two-valued logic
        auto left = dispatch(expr.ownership_left());
        switch (left) {
            case constant_true:
                return constant_true;

            case constant_false:
                ref = expr.release_right();
                return dispatch(ref);

            default:
                break;
        }
        auto right = dispatch(expr.ownership_right());
        switch (right) {
            case constant_true:
                return constant_true;

            case constant_false:
                ref = expr.release_left();
                return left;

            default:
                break;
        }

        /*
         * | && | T? | F? | U  | X  |
         * +----+----+----+----+----+
         * | T? | T? | T? | T? | T? | -> = T?
         * | F? | T? | F? | U  | X  | -> = right
         * | U  | T? | U  | U  | T? |
         * | X  | T? | X  | T? | X  |
         */
        switch (left) {
            case constant_true_or_unknown:
                return constant_true_or_unknown;

            case constant_false_or_unknown:
                return right;

            case constant_unknown:
                switch (right) {
                    case constant_true_or_unknown:
                    case not_sure:
                        return constant_true_or_unknown;
                    case constant_false_or_unknown:
                    case constant_unknown:
                        return constant_unknown;
                    default:
                        std::abort();
                }

            case not_sure:
                switch (right) {
                    case constant_false_or_unknown:
                    case constant_unknown:
                        return constant_true_or_unknown;
                    case constant_true_or_unknown:
                    case not_sure:
                        return not_sure;
                    default:
                        std::abort();
                }

            default:
                std::abort();
        }
    }
};

} // namespace

simplify_predicate_result simplify_predicate(expression_ref expression) {
    engine e {};
    return e.dispatch(std::move(expression));
}

} // namespace yugawara::analyzer::details
