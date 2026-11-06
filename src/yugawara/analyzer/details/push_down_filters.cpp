#include "push_down_filters.h"

#include <algorithm>
#include <functional>
#include <iterator>
#include <memory>
#include <utility>
#include <vector>

#include <tsl/hopscotch_set.h>

#include <takatori/scalar/binary.h>
#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include "boolean_constants.h"
#include "decompose_predicate.h"
#include "collect_stream_variables.h"
#include "stream_variable_flow_info.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::util::clone_unique;
using ::takatori::util::enum_tag;
using ::takatori::util::enum_tag_t;
using ::takatori::util::ownership_reference;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

namespace {

/**
 * @brief a conjunctive term of conditions, which is on the way to its destination.
 */
class term {
public:
    explicit term(std::unique_ptr<scalar::expression> expression) :
        expression_ { std::move(expression) }
    {
        collect_stream_variables(*expression_, [this](descriptor::variable const& v) {
            uses_.emplace(v);
        });
    }

    [[nodiscard]] term clone() const {
        return term { clone_unique(*expression_) };
    }

    [[nodiscard]] auto const& uses() const noexcept {
        return uses_;
    }

    [[nodiscard]] bool use(descriptor::variable const& variable) const {
        return uses_.contains(variable);
    }

    /**
     * @brief returns whether or not this term only uses the given variables.
     * @param variables the variables
     * @return true if this term does not use any other variables
     * @return false otherwise
     */
    [[nodiscard]] bool use_only(std::vector<descriptor::variable> const& variables) const {
        return std::all_of(uses_.begin(), uses_.end(), [&](auto&& use) {
            return std::find(variables.begin(), variables.end(), use) != variables.end();
        });
    }

    [[nodiscard]] std::unique_ptr<scalar::expression> release() noexcept {
        return std::move(expression_);
    }

private:
    std::unique_ptr<scalar::expression> expression_;
    ::tsl::hopscotch_set<
            descriptor::variable,
            std::hash<descriptor::variable>,
            std::equal_to<>> uses_;
};

using term_list = std::vector<term>;

/**
 * @brief extracts the terms which satisfy the given predicate.
 * @details The other terms stay in the given list. That is, the terms never move unless they are extracted.
 * @param terms the terms
 * @param predicate the predicate
 * @return the extracted terms
 */
template<class Predicate>
[[nodiscard]] term_list extract(term_list& terms, Predicate&& predicate) {
    term_list extracted {};
    term_list rest {};
    for (auto&& element : terms) {
        (predicate(std::as_const(element)) ? extracted : rest).emplace_back(std::move(element));
    }
    terms = std::move(rest);
    return extracted;
}

void append(term_list& terms, term_list others) {
    terms.insert(terms.end(), std::make_move_iterator(others.begin()), std::make_move_iterator(others.end()));
}

/**
 * @brief takes all conjunctive terms out of the given condition.
 * @details The rest of the condition only consists of the constant `TRUE`.
 * @param condition the condition, may be empty
 * @return the terms in the condition
 */
[[nodiscard]] term_list take_terms(ownership_reference<scalar::expression> condition) {
    term_list results {};
    if (condition) {
        decompose_predicate(std::move(condition), [&](ownership_reference<scalar::expression>&& element) {
            results.emplace_back(element.exchange(make_boolean_expression(true)));
        });
    }
    return results;
}

/**
 * @brief builds a conjunction of the given terms.
 * @param terms the terms
 * @return the conjunction of the terms
 * @return empty if there are no terms
 */
[[nodiscard]] std::unique_ptr<scalar::expression> make_conjunction(term_list terms) {
    std::unique_ptr<scalar::expression> result {};
    for (auto&& element : terms) {
        if (result) {
            result = std::make_unique<scalar::binary>(
                    scalar::binary_operator::conditional_and,
                    std::move(result),
                    element.release());
        } else {
            result = element.release();
        }
    }
    return result;
}

// NOTE: avoid error of Boost polymorphic_allocator, this is equivalent to std::pair
class task_info {
public:
    explicit task_info(relation::expression& expression, term_list terms) :
        expression_ { std::addressof(expression) },
        terms_ { std::move(terms) }
    {}

    [[nodiscard]] relation::expression& expression() noexcept {
        return *expression_;
    }

    [[nodiscard]] term_list& terms() noexcept {
        return terms_;
    }

private:
    relation::expression* expression_;
    term_list terms_;
};

/**
 * @brief moves the terms of conditions to upstream.
 * @details Each operator receives the terms from its downstream (`incoming`), and then decides where to
 *      deploy each of them, by the following operations:
 *
 *      - extract() - takes the terms to move out of the list. The other terms stay in the list.
 *      - forward() - carries the terms to the upstream of the input.
 *      - place_downstream() - places the terms at the output of the operator.
 *      - make_conjunction() - builds the join condition from the terms to place in it.
 */
class engine {
public:
    explicit engine(relation::graph_type& graph) :
        graph_ { graph }
    {}

    void process() {
        relation::enumerate_bottom(graph_, [&](relation::expression& expr) {
            schedule(expr, {});
        });
        while (!tasks_.empty()) {
            auto task = std::move(tasks_.back());
            tasks_.pop_back();
            relation::intermediate::dispatch(*this, task.expression(), std::move(task.terms()));
        }
    }

    void operator()(relation::find& expr, term_list incoming) {
        place_downstream(expr.output(), std::move(incoming));
        // no more upstreams
    }

    void operator()(relation::scan& expr, term_list incoming) {
        place_downstream(expr.output(), std::move(incoming));
        // no more upstreams
    }

    void operator()(relation::values& expr, term_list incoming) {
        place_downstream(expr.output(), std::move(incoming));
        // no more upstreams
    }

    void operator()(relation::intermediate::join& expr, term_list incoming) {
        relation::dispatch(*this, expr.operator_kind(), expr, std::move(incoming));
    }

    void operator()(enum_tag_t<relation::join_kind::inner>, relation::intermediate::join& expr, term_list incoming) {
        // NOTE: inner joins can consider the join condition terms as same as incoming terms
        auto own = take_terms(expr.ownership_condition());
        term_list to_left {};
        term_list to_right {};
        extract_for_inputs(incoming, expr, to_left, to_right);
        extract_for_inputs(own, expr, to_left, to_right);

        // the other terms need both inputs
        append(own, std::move(incoming));
        expr.condition(make_conjunction(std::move(own)));

        forward(expr.left(), std::move(to_left));
        forward(expr.right(), std::move(to_right));
    }

    void operator()(
            enum_tag_t<relation::join_kind::left_outer>,
            relation::intermediate::join& expr,
            term_list incoming) {
        /*
         * NOTE: the terms of the join condition which only refer the right columns can be pushed down into
         * the right input, because they only restrict the right rows to be joined, that is,
         * "L LEFT JOIN R ON c AND p(R)" is equivalent to "L LEFT JOIN (R WHERE p(R)) ON c".
         * On the other hand, the other terms of the join condition must be kept, because they decide
         * which left rows are padded with nulls (the left-only terms must not be pushed down into the left input).
         * Also, the incoming terms which refer the right columns must not be pushed down into the join condition,
         * because they remove the rows padded with nulls.
         */
        auto own = take_terms(expr.ownership_condition());
        auto to_left = extract(incoming, available_on(expr.left()));
        auto to_right = extract(own, available_on(expr.right()));

        expr.condition(make_conjunction(std::move(own)));
        place_downstream(expr.output(), std::move(incoming));

        forward(expr.left(), std::move(to_left));
        forward(expr.right(), std::move(to_right));
    }

    void operator()(
            enum_tag_t<relation::join_kind::full_outer>,
            relation::intermediate::join& expr,
            term_list incoming) {
        // NOTE: outer join does not decompose the join condition
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.left(), {});
        forward(expr.right(), {});
    }

    void operator()(
            enum_tag_t<relation::join_kind::left_outer_at_most_one>,
            relation::intermediate::join& expr,
            term_list incoming) {
        // same as left outer join
        operator()(enum_tag<relation::join_kind::left_outer>, expr, std::move(incoming));
    }

    void operator()(enum_tag_t<relation::join_kind::semi>, relation::intermediate::join& expr, term_list incoming) {
        // same as inner join
        operator()(enum_tag<relation::join_kind::inner>, expr, std::move(incoming));
    }

    void operator()(enum_tag_t<relation::join_kind::anti>, relation::intermediate::join& expr, term_list incoming) {
        /*
         * NOTE: same as left outer join:
         * - the right-only terms of the join condition only restrict the right rows to be joined
         * - the other terms of the join condition must be kept, because they decide which left rows are emitted
         * - the incoming terms only refer the left columns, because anti join does not emit the right columns
         * Note that, this is not true for NAAJ (Null-Aware Anti-Join), whose result depends on whether the right
         * input has nulls.
         */
        operator()(enum_tag<relation::join_kind::left_outer>, expr, std::move(incoming));
    }

    void operator()(relation::join_find& expr, term_list incoming) {
        // NOTE: index join is not supported here, it will be generated in the later optimization phase
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.left(), {});
    }

    void operator()(relation::join_scan& expr, term_list incoming) {
        // NOTE: index join is not supported here, it will be generated in the later optimization phase
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.left(), {});
    }

    void operator()(relation::apply& expr, term_list incoming) {
        process_column_definitions(expr, std::move(incoming));
    }

    void operator()(relation::project& expr, term_list incoming) {
        process_column_definitions(expr, std::move(incoming));
    }

    void operator()(relation::filter& expr, term_list incoming) {
        auto own = take_terms(expr.ownership_condition());
        // NOTE: the filter must have its condition, even if all terms go away
        expr.condition(make_boolean_expression(true));

        append(incoming, std::move(own));
        forward(expr.input(), std::move(incoming));
    }

    void operator()(relation::identify& expr, term_list incoming) {
        // NOTE: predicates may not use row_id column in most cases..
        auto to_input = extract(incoming, [&](term const& element) {
            return !element.use(expr.variable());
        });
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.input(), std::move(to_input));
    }

    void operator()(relation::intermediate::aggregate& expr, term_list incoming) {
        if (expr.group_keys().empty()) {
            // NOTE: the aggregation without group keys emits a row even if its input is empty,
            // so that even the constant terms must not go over it
            place_downstream(expr.output(), std::move(incoming));
            forward(expr.input(), {});
            return;
        }
        process_group_like(expr, std::move(incoming));
    }

    void operator()(relation::intermediate::distinct& expr, term_list incoming) {
        for (auto&& element : incoming) {
            if (!element.use_only(expr.group_keys())) {
                // found use other than the group key, put a copy of selection
                place_downstream(expr.output(), element.clone());
            }
        }
        forward(expr.input(), std::move(incoming));
    }

    void operator()(relation::intermediate::limit& expr, term_list incoming) {
        // NOTE: the terms must not go over the limit unless they only use the group keys,
        // because they change which rows are in the row slice of each group
        process_group_like(expr, std::move(incoming));
    }

    void operator()(relation::intermediate::union_& expr, term_list incoming) {
        // FIXME: impl
        // rewrite variables and carry into left or right
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.left(), {});
        forward(expr.right(), {});
    }

    void operator()(relation::intermediate::intersection& expr, term_list incoming) {
        // FIXME: check for distinct
        forward(expr.left(), std::move(incoming));
        forward(expr.right(), {});
    }

    void operator()(relation::intermediate::difference& expr, term_list incoming) {
        // FIXME: check for distinct
        forward(expr.left(), std::move(incoming));
        forward(expr.right(), {});
    }

    void operator()(relation::emit& expr, term_list incoming) {
        forward(expr.input(), std::move(incoming));
    }

    void operator()(relation::write& expr, term_list incoming) {
        forward(expr.input(), std::move(incoming));
    }

    void operator()(relation::intermediate::escape& expr, term_list incoming) {
        // selection cannot push over escape
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.input(), {});
    }

    void operator()(relation::buffer& expr, term_list const&) {
        throw_exception(std::domain_error(string_builder {}
                << "unsupported operator: "
                << expr
                << string_builder::to_string));
    }

    void operator()(relation::intermediate::extension& expr, term_list const&) {
        throw_exception(std::domain_error(string_builder {}
                << "unsupported operator: "
                << expr
                << string_builder::to_string));
    }

private:
    relation::graph_type& graph_;
    std::vector<task_info> tasks_;
    stream_variable_flow_info flow_info_; // FIXME: reuse flow info?

    void schedule(relation::expression& expr, term_list terms) {
        tasks_.emplace_back(expr, std::move(terms));
    }

    /**
     * @brief carries the terms to the upstream of the given input.
     * @param input the input port
     * @param terms the terms
     */
    void forward(relation::expression::input_port_type& input, term_list terms) {
        if (!input.opposite()) {
            throw_exception(std::domain_error(string_builder {}
                    << "orphaned port: "
                    << input
                    << string_builder::to_string));
        }
        auto&& next = input.opposite()->owner();

        // NOTE: always place all terms before buffer operators, and never process them
        if (next.kind() == relation::buffer::tag) {
            place_downstream(*input.opposite(), std::move(terms));
            return;
        }

        // reschedule with the upstream operator
        schedule(next, std::move(terms));
    }

    /**
     * @brief places a filter of the term just after the given output.
     * @param output the output port
     * @param element the term
     */
    void place_downstream(relation::expression::output_port_type& output, term element) {
        auto&& selection = graph_.emplace<relation::filter>(element.release());
        auto&& downstream = *output.opposite();
        output.disconnect_from(downstream);
        selection.input().connect_to(output);
        selection.output().connect_to(downstream);
    }

    /**
     * @brief places filters of the terms just after the given output.
     * @param output the output port
     * @param terms the terms
     */
    void place_downstream(relation::expression::output_port_type& output, term_list terms) {
        for (auto&& element : terms) {
            place_downstream(output, std::move(element));
        }
    }

    /**
     * @brief returns a predicate which tests whether or not the term can be evaluated on the given input.
     * @param input the input port
     * @return the predicate
     */
    [[nodiscard]] std::function<bool(term const&)> available_on(relation::expression::input_port_type const& input) {
        return [this, &input](term const& element) {
            auto&& uses = element.uses();
            return std::all_of(uses.begin(), uses.end(), [&](auto&& use) {
                return flow_info_.find(use, input).has_value();
            });
        };
    }

    /**
     * @brief extracts the terms which can be evaluated on the left or right input of the join.
     * @details The terms available on both inputs are carried to each of them.
     * @param terms the terms
     * @param expr the target join
     * @param to_left the destination of the terms for the left input
     * @param to_right the destination of the terms for the right input
     */
    void extract_for_inputs(
            term_list& terms,
            relation::intermediate::join const& expr,
            term_list& to_left,
            term_list& to_right) {
        auto left = extract(terms, available_on(expr.left()));
        auto both = extract(left, available_on(expr.right()));
        for (auto&& element : both) {
            to_right.emplace_back(element.clone());
        }
        append(to_left, std::move(left));
        append(to_left, std::move(both));
        append(to_right, extract(terms, available_on(expr.right())));
    }

    template<class Expr>
    void process_column_definitions(Expr& expr, term_list incoming) {
        // pass terms only using the input columns
        auto to_input = extract(incoming, [&](term const& element) {
            auto&& columns = expr.columns();
            return std::none_of(columns.begin(), columns.end(), [&](auto&& column) {
                return element.use(column.variable());
            });
        });
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.input(), std::move(to_input));
    }

    template<class Expr>
    void process_group_like(Expr& expr, term_list incoming) {
        // pass terms only using the group keys
        auto to_input = extract(incoming, [&](term const& element) {
            return element.use_only(expr.group_keys());
        });
        place_downstream(expr.output(), std::move(incoming));
        forward(expr.input(), std::move(to_input));
    }
};

} // namespace

void push_down_selections(relation::graph_type& graph) {
    engine e { graph };
    e.process();
}

} // namespace yugawara::analyzer::details
