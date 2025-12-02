#include "decompose_disjunction_range.h"

#include <takatori/descriptor/variable.h>

#include <takatori/scalar/dispatch.h>

#include <takatori/relation/filter.h>

#include <takatori/util/downcast.h>

#include <yugawara/binding/extract.h>

#include <yugawara/storage/column.h>

#include "range_hint.h"

namespace yugawara::analyzer::details {

using ::takatori::util::unsafe_downcast;

namespace {

class engine {
public:

    std::vector<std::unique_ptr<takatori::scalar::expression>> process(takatori::scalar::expression const& expr) {
        if (expr.kind() == ::takatori::scalar::binary::tag) {
            // check if the root node is a disjunction
            auto&& binary = unsafe_downcast<::takatori::scalar::binary>(expr);
            if (binary.operator_kind() == ::takatori::scalar::binary_operator::conditional_or) {
                auto result = dispatch(expr);
                return to_vector(std::move(result));
            }
        }
        return {};
    }

    range_hint_map operator()(::takatori::scalar::expression const& expr) {
        // default: do nothing
        (void) expr;
        return {};
    }

    range_hint_map operator()(::takatori::scalar::binary const& expr) {
        if (expr.operator_kind() == ::takatori::scalar::binary_operator::conditional_or) {
            // disjunction: collect range hints from both sides
            auto left = dispatch(*expr.optional_left());
            auto right = dispatch(*expr.optional_right());
            left.union_merge(std::move(right));
            return left;
        }
        if (expr.operator_kind() == ::takatori::scalar::binary_operator::conditional_and) {
            // conjunction: collect range hints from both sides
            auto left = dispatch(*expr.optional_left());
            auto right = dispatch(*expr.optional_right());
            left.intersect_merge(std::move(right));
            return left;
        }
        // other binary expressions: do nothing
        return {};
    }

    range_hint_map operator()(::takatori::scalar::compare const& expr) {
        if (expr.operator_kind() == ::takatori::scalar::comparison_operator::not_equal) {
            return {};
        }
        if (is_column_access(expr.left())) {
            auto&& column = unsafe_downcast<::takatori::scalar::variable_reference>(expr.left());
            auto comparator = expr.operator_kind();
            return extract(column.variable(), expr.right(), comparator);
        }
        if (is_column_access(expr.right())) {
            auto&& column = unsafe_downcast<::takatori::scalar::variable_reference>(expr.right());
            auto comparator = ::takatori::scalar::transpose(expr.operator_kind());
            return extract(column.variable(), expr.left(), comparator);
        }
        return {};
    }

private:
    range_hint_map dispatch(::takatori::scalar::expression const& expr) {
        auto result = ::takatori::scalar::dispatch(*this, expr);
        return result;
    }

    bool is_column_access(::takatori::scalar::expression const& expr) {
        if (expr.kind() != ::takatori::scalar::variable_reference::tag) {
            return false;
        }
        auto&& var_ref = unsafe_downcast<::takatori::scalar::variable_reference>(expr);
        return binding::kind_of(var_ref.variable()) == binding::variable_info_kind::stream_variable;
    }

    range_hint_map extract(
            ::takatori::descriptor::variable const& column,
            ::takatori::scalar::expression const& operand,
            ::takatori::scalar::comparison_operator comparator) {
        if (operand.kind() == ::takatori::scalar::immediate::tag) {
            auto&& value = unsafe_downcast<::takatori::scalar::immediate>(operand);
            range_hint_map result {};
            auto&& entry = result.get(column);
            using kind = ::takatori::scalar::comparison_operator;
            switch (comparator) {
                case kind::equal:
                    entry.intersect_lower(value, true);
                    entry.intersect_upper(value, true);
                    break;
                case kind::less: // column < value
                    entry.intersect_upper(value, false);
                    break;
                case kind::greater: // column > value
                    entry.intersect_lower(value, false);
                    break;
                case kind::less_equal: // column <= value
                    entry.intersect_upper(value, true);
                    break;
                case kind::greater_equal: // column >= value
                    entry.intersect_lower(value, true);
                    break;
                default:
                    break;
            }
            return result;
        }
        if (operand.kind() == ::takatori::scalar::variable_reference::tag) {
            auto&& var_ref = unsafe_downcast<::takatori::scalar::variable_reference>(operand);
            if (binding::kind_of(var_ref.variable()) == binding::variable_info_kind::external_variable) {
                range_hint_map result {};
                auto&& entry = result.get(column);
                using kind = ::takatori::scalar::comparison_operator;
                switch (comparator) {
                    case kind::equal:
                        entry.intersect_lower(var_ref.variable(), true);
                        entry.intersect_upper(var_ref.variable(), true);
                        break;
                    case kind::less: // column < variable
                        entry.intersect_upper(var_ref.variable(), false);
                        break;
                    case kind::greater: // column > variable
                        entry.intersect_lower(var_ref.variable(), false);
                        break;
                    case kind::less_equal: // column <= variable
                        entry.intersect_upper(var_ref.variable(), true);
                        break;
                    case kind::greater_equal: // column >= variable
                        entry.intersect_lower(var_ref.variable(), true);
                        break;
                    default:
                        break;
                }
                return result;
            }
            return {};
        }
        return {};
    }

    [[nodiscard]] std::vector<std::unique_ptr<takatori::scalar::expression>> to_vector(range_hint_map&& map) {
        std::vector<std::unique_ptr<takatori::scalar::expression>> results;
        map.consume([&](::takatori::descriptor::variable const& key, range_hint_entry&& value) -> void {
            using cmp = ::takatori::scalar::comparison_operator;
            if (value.lower_type() != range_hint_type::infinity) {
                // construct lower bound expression (value </<= column)
                auto expr = std::make_unique<::takatori::scalar::compare>(
                        value.lower_type() == range_hint_type::inclusive ? cmp::less_equal : cmp::less,
                        to_expression(std::move(value.lower_value())),
                        std::make_unique<::takatori::scalar::variable_reference>(key));
                results.emplace_back(std::move(expr));
            }
            if (value.upper_type() != range_hint_type::infinity) {
                // construct upper bound expression (column </<= value)
                auto expr = std::make_unique<::takatori::scalar::compare>(
                        value.upper_type() == range_hint_type::inclusive ? cmp::less_equal : cmp::less,
                        std::make_unique<::takatori::scalar::variable_reference>(key),
                        to_expression(std::move(value.upper_value())));
                results.emplace_back(std::move(expr));
            }
        });
        return results;
    }

    [[nodiscard]] std::unique_ptr<::takatori::scalar::expression> to_expression(range_hint_entry::value_type&& value) {
        if (std::holds_alternative<range_hint_entry::variable_type>(value)) {
            auto&& var = std::get<range_hint_entry::variable_type>(value);
            return std::make_unique<::takatori::scalar::variable_reference>(std::move(var));
        }
        if (std::holds_alternative<range_hint_entry::immediate_type>(value)) {
            auto&& imm = std::get<range_hint_entry::immediate_type>(value);
            return std::move(imm);
        }
        // should not reach here
        std::abort();
    }
};

} // namespace

void decompose_disjunction_range(takatori::relation::graph_type& graph) {
    for (auto&& expr : graph) {
        if (expr.kind() == ::takatori::relation::filter::tag) {
            auto&& filter = unsafe_downcast<takatori::relation::filter>(expr);
            auto hints = decompose_disjunction_range_collect(filter.condition());
            if (!hints.empty()) {
                std::unique_ptr<::takatori::scalar::expression> current = filter.release_condition();
                for (auto&& term : hints) {
                    current = std::make_unique<::takatori::scalar::binary>(
                            ::takatori::scalar::binary_operator::conditional_and,
                            std::move(current),
                            std::move(term));
                }
                filter.condition(std::move(current));
            }
        }
    }
}

std::vector<std::unique_ptr<takatori::scalar::expression>> decompose_disjunction_range_collect(
        takatori::scalar::expression const& expr) {
    engine e {};
    auto result = e.process(expr);
    return result;
}

} // namespace yugawara::analyzer::details
