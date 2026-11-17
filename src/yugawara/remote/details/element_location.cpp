#include "element_location.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/values.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/identify.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>
#include <takatori/relation/step/intersection.h>
#include <takatori/relation/step/difference.h>
#include <takatori/relation/step/flatten.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/find.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/step/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/step/aggregate.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/write.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/take_group.h>
#include <takatori/relation/step/take_cogroup.h>
#include <takatori/relation/expression_kind.h>
#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/relation/step/dispatch.h>

#include <takatori/scalar/function_call.h>
#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/extension.h>
#include <takatori/scalar/walk.h>

#include <takatori/plan/dispatch.h>

#include <takatori/statement/dispatch.h>

#include <takatori/descriptor/relation.h>
#include <takatori/descriptor/storage.h>
#include <takatori/descriptor/function.h>
#include <takatori/descriptor/aggregate_function.h>
#include <takatori/descriptor/variable.h>

#include <boost/assert.hpp>

#include <yugawara/binding/extract.h>
#include <yugawara/storage/index.h>
#include <yugawara/function/declaration.h>
#include <yugawara/aggregate/declaration.h>
#include <yugawara/variable/declaration.h>

namespace yugawara::remote::details {

namespace descriptor = ::takatori::descriptor;
namespace tscalar = ::takatori::scalar;
namespace trelation = ::takatori::relation;
namespace tplan = ::takatori::plan;
namespace tstatement = ::takatori::statement;

using location_id_set_type = element_location::location_id_set_type;

namespace {

location_id_set_type local_location() {
    return { remote::location_info::local_location_id };
}

remote::symbol_location_query const& require_query(remote::location_options const& options) noexcept {
    BOOST_ASSERT(options.symbol_location_query()); // NOLINT
    return *options.symbol_location_query();
}

void intersect(location_id_set_type& target, location_id_set_type const& other) {
    location_id_set_type intersection {};
    std::set_intersection(
            target.begin(), target.end(),
            other.begin(), other.end(),
            std::inserter(intersection, intersection.end()));
    target = std::move(intersection);
}

/**
 * @brief accepts constraints of locations, and provides the locations which satisfy all of them.
 * @details The constraints are the referred symbols (e.g. tables), the locations of scalar expressions,
 *      and the local location.
 */
class location_constraints {
public:
    explicit location_constraints(remote::symbol_location_query const& query) :
        query_ { query },
        locations_ { query.available_location_ids() }
    {}

    /**
     * @brief adds the location of the given relation.
     * @details The location of each table index is the location of its table.
     *      The upstream exchanges and the external relations never restrict the locations,
     *      because the locations of the exchanges are decided as the individual steps,
     *      and the external relations are the substitutes of the exchanges, which are managed by the
     *      runtime environment.
     *      The other relations restrict the locations to the local location.
     * @param descriptor the target relation
     */
    void add(descriptor::relation const& descriptor) {
        if (auto index = binding::extract_if<storage::index>(descriptor)) {
            add(index->table());
            return;
        }
        if (auto kind = binding::kind_of(descriptor);
                kind == binding::relation_info_kind::exchange
                || kind == binding::relation_info_kind::external) {
            return;
        }
        intersect(locations_, local_location());
    }

    void add(descriptor::storage const& descriptor) {
        if (auto table = binding::extract_if<storage::table>(descriptor)) {
            add(*table);
            return;
        }
        intersect(locations_, local_location());
    }

    /**
     * @brief adds the locations of the given function.
     * @details If it is not a function declaration, this restricts the locations to the local location.
     * @param descriptor the target function
     */
    void add(descriptor::function const& descriptor) {
        if (auto function = binding::extract_if<function::declaration>(descriptor)) {
            intersect(locations_, query_.find(*function));
            return;
        }
        restrict_to_local();
    }

    /**
     * @brief adds the locations of the given aggregate function.
     * @details If it is not an aggregate function declaration,
     *      this restricts the locations to the local location.
     * @param descriptor the target aggregate function
     */
    void add(descriptor::aggregate_function const& descriptor) {
        if (auto function = binding::extract_if<aggregate::declaration>(descriptor)) {
            intersect(locations_, query_.find(*function));
            return;
        }
        restrict_to_local();
    }

    /**
     * @brief adds the locations of the given variable, only if it is an external variable (e.g. host variables).
     * @details The other variables (e.g. stream variables) never restrict the locations.
     *      If the external variable is not a variable declaration,
     *      this restricts the locations to the local location.
     * @param descriptor the target variable
     */
    void add(descriptor::variable const& descriptor) {
        if (binding::kind_of(descriptor) != binding::variable_info_kind::external_variable) {
            return;
        }
        if (auto variable = binding::extract_if<variable::declaration>(descriptor)) {
            intersect(locations_, query_.find(*variable));
            return;
        }
        restrict_to_local();
    }

    void restrict_to_local() {
        intersect(locations_, local_location());
    }

    /**
     * @brief restricts the locations to the local location, if the given kind is not in the given set.
     * @tparam Set the kind set type
     * @param kinds the kinds which can be executed in locations other than the local
     * @param kind the kind of the target element
     */
    template<class Set>
    void add_kind(Set const& kinds, typename Set::value_type kind) {
        if (!kinds.contains(kind)) {
            restrict_to_local();
        }
    }

    void add(location_id_set_type const& locations) {
        intersect(locations_, locations);
    }

    template<class T>
    void add_privilege_elements(T const& statement) {
        for (auto&& element : statement.elements()) {
            add(element.table());
        }
    }

    /**
     * @brief returns the locations which satisfy all the given constraints.
     * @return the intersection of the available locations and the given constraints
     */
    [[nodiscard]] location_id_set_type release() && {
        return std::move(locations_);
    }

private:
    remote::symbol_location_query const& query_;
    location_id_set_type locations_;

    void add(storage::table const& table) {
        intersect(locations_, query_.find(table));
    }
};

/**
 * @brief decomposes an element (statement, step, operator, or scalar expression),
 *      and collects its constraints of locations.
 * @details Each element is restricted by its kind, by the symbols it refers, and by the elements in it.
 *      The operators and statements not listed here may have scalar expressions or symbols not inspected here,
 *      so that they are restricted to the local location.
 */
class location_collector {
public:
    /**
     * @brief creates a new instance.
     * @param options the location options, which must have a symbol location query
     * @param constraints the destination of the collected constraints
     */
    explicit location_collector(
            remote::location_options const& options,
            location_constraints& constraints) noexcept :
        options_ { options },
        query_ { require_query(options) },
        constraints_ { constraints }
    {}

    /**
     * @brief collects the constraints of the given statement.
     * @param statement the target statement
     */
    void collect(tstatement::statement const& statement) {
        constraints_.add_kind(options_.remote_statement_kinds(), statement.kind());
        tstatement::dispatch(*this, statement);
    }

    /**
     * @brief collects the constraints of the given step.
     * @param step the target step
     */
    void collect(tplan::step const& step) {
        constraints_.add_kind(options_.remote_step_kinds(), step.kind());
        tplan::dispatch(*this, step);
    }

    /**
     * @brief collects the constraints of the given operator.
     * @param expression the target operator
     */
    void collect(trelation::expression const& expression) {
        constraints_.add_kind(options_.remote_relation_expression_kinds(), expression.kind());
        if (trelation::is_available_in_intermediate_plan(expression.kind())) {
            trelation::intermediate::dispatch(*this, expression);
        } else {
            trelation::step::dispatch(*this, expression);
        }
    }

    /**
     * @brief collects the constraints of the given scalar expression.
     * @param expression the target scalar expression
     */
    void collect(tscalar::expression const& expression) {
        tscalar::walk(*this, expression);
    }

    // statements

    void operator()(tstatement::statement const& statement) {
        (void) statement;
        // NOTE: the symbols in the other statements are not inspected
        constraints_.restrict_to_local();
    }

    void operator()(tstatement::execute const& statement) {
        // NOTE: the steps may be executed in different locations, so that the result may be empty
        for (auto&& step : statement.execution_plan()) {
            collect(step);
        }
    }

    void operator()(tstatement::write const& statement) {
        constraints_.add(statement.destination());
        for (auto&& tuple : statement.tuples()) {
            for (auto&& element : tuple.elements()) {
                collect(element);
            }
        }
    }

    void operator()(tstatement::drop_table const& statement) {
        constraints_.add(statement.target());
    }

    void operator()(tstatement::rename_table const& statement) {
        constraints_.add(statement.target());
    }

    void operator()(tstatement::rename_column const& statement) {
        constraints_.add(statement.table());
    }

    void operator()(tstatement::create_index const& statement) {
        constraints_.add(statement.definition());
    }

    void operator()(tstatement::drop_index const& statement) {
        constraints_.add(statement.target());
    }

    void operator()(tstatement::rename_index const& statement) {
        constraints_.add(statement.target());
    }

    void operator()(tstatement::truncate_table const& statement) {
        constraints_.add(statement.target());
    }

    void operator()(tstatement::grant_table const& statement) {
        constraints_.add_privilege_elements(statement);
    }

    void operator()(tstatement::revoke_table const& statement) {
        constraints_.add_privilege_elements(statement);
    }

    // steps

    void operator()(tplan::process const& step) {
        for (auto&& expression : step.operators()) {
            collect(expression);
        }
    }

    void operator()(tplan::exchange const& step) {
        (void) step;
        // NOTE: the contents of the other exchanges are not inspected
        constraints_.restrict_to_local();
    }

    void operator()(tplan::aggregate const& step) {
        add_aggregations(step.aggregations());
    }

    void operator()(tplan::forward const& step) {
        add_constant_value(step.row_slice().start());
        add_constant_value(step.row_slice().count());
    }

    void operator()(tplan::group const& step) {
        add_constant_value(step.row_slice().start());
        add_constant_value(step.row_slice().count());
    }

    // exchanges which never have scalar expressions nor symbols
    void operator()(tplan::broadcast const& step) noexcept {
        (void) step;
    }

    void operator()(tplan::discard const& step) noexcept {
        (void) step;
    }

    // operators

    void operator()(trelation::expression const& expression) {
        (void) expression;
        // NOTE: the scalar expressions and symbols in the other operators are not inspected yet
        constraints_.restrict_to_local();
    }

    void operator()(trelation::scan const& expression) {
        constraints_.add(expression.source());
        add_keys(expression.lower().keys());
        add_keys(expression.upper().keys());
        add_constant_value(expression.row_slice().start());
        add_constant_value(expression.row_slice().count());
    }

    void operator()(trelation::find const& expression) {
        constraints_.add(expression.source());
        add_keys(expression.keys());
    }

    void operator()(trelation::join_find const& expression) {
        constraints_.add(expression.source());
        add_keys(expression.keys());
        add_condition(expression.condition());
    }

    void operator()(trelation::join_scan const& expression) {
        constraints_.add(expression.source());
        add_keys(expression.lower().keys());
        add_keys(expression.upper().keys());
        add_condition(expression.condition());
    }

    void operator()(trelation::intermediate::join const& expression) {
        add_keys(expression.lower().keys());
        add_keys(expression.upper().keys());
        add_condition(expression.condition());
    }

    void operator()(trelation::step::join const& expression) {
        add_condition(expression.condition());
    }

    void operator()(trelation::intermediate::limit const& expression) {
        add_variables(expression.group_keys());
        for (auto&& key : expression.sort_keys()) {
            constraints_.add(key.variable());
        }
        add_constant_value(expression.row_slice().start());
        add_constant_value(expression.row_slice().count());
    }

    void operator()(trelation::apply const& expression) {
        constraints_.add(expression.function());
        for (auto&& argument : expression.arguments()) {
            collect(argument);
        }
    }

    void operator()(trelation::intermediate::aggregate const& expression) {
        add_variables(expression.group_keys());
        add_aggregations(expression.columns());
    }

    void operator()(trelation::step::aggregate const& expression) {
        add_aggregations(expression.columns());
    }

    void operator()(trelation::write const& expression) {
        constraints_.add(expression.destination());
        for (auto&& key : expression.keys()) {
            constraints_.add(key.source());
        }
        for (auto&& column : expression.columns()) {
            constraints_.add(column.source());
        }
    }

    void operator()(trelation::project const& expression) {
        for (auto&& column : expression.columns()) {
            collect(column.value());
        }
    }

    void operator()(trelation::values const& expression) {
        for (auto&& row : expression.rows()) {
            for (auto&& element : row.elements()) {
                collect(element);
            }
        }
    }

    void operator()(trelation::filter const& expression) {
        tscalar::walk(*this, expression.condition());
    }

    // operators which refer to variables only
    void operator()(trelation::emit const& expression) {
        for (auto&& column : expression.columns()) {
            constraints_.add(column.source());
        }
    }

    void operator()(trelation::intermediate::distinct const& expression) {
        add_variables(expression.group_keys());
    }

    void operator()(trelation::intermediate::union_ const& expression) {
        for (auto&& mapping : expression.mappings()) {
            if (mapping.left()) {
                constraints_.add(*mapping.left());
            }
            if (mapping.right()) {
                constraints_.add(*mapping.right());
            }
        }
    }

    void operator()(trelation::intermediate::intersection const& expression) {
        add_key_pairs(expression.group_key_pairs());
    }

    void operator()(trelation::intermediate::difference const& expression) {
        add_key_pairs(expression.group_key_pairs());
    }

    void operator()(trelation::intermediate::escape const& expression) {
        for (auto&& mapping : expression.mappings()) {
            constraints_.add(mapping.source());
        }
    }

    void operator()(trelation::step::offer const& expression) {
        for (auto&& column : expression.columns()) {
            constraints_.add(column.source());
        }
    }

    // operators which never have scalar expressions nor symbols
    void operator()(trelation::buffer const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::identify const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::step::intersection const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::step::difference const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::step::flatten const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::step::take_flat const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::step::take_group const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::step::take_cogroup const& expression) noexcept {
        (void) expression;
    }

    // scalar expressions

    void operator()(tscalar::expression const& expression) {
        check_kind(expression);
    }

    void operator()(tscalar::function_call const& expression) {
        check_kind(expression);
        constraints_.add(expression.function());
    }

    void operator()(tscalar::variable_reference const& expression) {
        check_kind(expression);
        constraints_.add(expression.variable());
    }

    void operator()(tscalar::extension const& expression) {
        (void) expression;
        // NOTE: the contents of extensions are not inspected
        constraints_.restrict_to_local();
    }

private:
    remote::location_options const& options_;
    remote::symbol_location_query const& query_;
    location_constraints& constraints_;

    void check_kind(tscalar::expression const& expression) {
        constraints_.add_kind(options_.remote_scalar_expression_kinds(), expression.kind());
    }

    /**
     * @brief adds the locations of the given variables.
     * @details Operators may refer external variables directly (not in scalar expressions),
     *      e.g. if optimizations inline external variables into the columns of operators.
     * @param variables the target variables
     */
    template<class Variables>
    void add_variables(Variables const& variables) {
        for (auto&& variable : variables) {
            constraints_.add(variable);
        }
    }

    template<class KeyPairs>
    void add_key_pairs(KeyPairs const& pairs) {
        for (auto&& pair : pairs) {
            constraints_.add(pair.left());
            constraints_.add(pair.right());
        }
    }

    template<class Aggregations>
    void add_aggregations(Aggregations const& aggregations) {
        for (auto&& aggregation : aggregations) {
            add_variables(aggregation.arguments());
            constraints_.add(aggregation.function());
        }
    }

    void add_condition(::takatori::util::optional_ptr<tscalar::expression const> condition) {
        if (condition) {
            collect(*condition);
        }
    }

    template<class Keys>
    void add_keys(Keys const& keys) {
        for (auto&& key : keys) {
            tscalar::walk(*this, key.value());
        }
    }

    template<class Value>
    void add_constant_value(Value const& value) {
        if (value.has_variable()) {
            constraints_.add(value.variable());
        }
    }
};

template<class T>
location_id_set_type collect_locations(remote::location_options const& options, T const& element) {
    location_constraints constraints { require_query(options) };
    location_collector collector { options, constraints };
    collector.collect(element);
    return std::move(constraints).release();
}

} // namespace

element_location::element_location(remote::location_options options) noexcept :
    options_ { std::move(options) },
    query_ { require_query(options_) }
{}

location_id_set_type element_location::all() const {
    return query_.available_location_ids();
}

location_id_set_type element_location::operator()(tscalar::expression const& expression) const {
    return collect_locations(options_, expression);
}

location_id_set_type element_location::operator()(trelation::expression_kind kind) const {
    location_constraints constraints { query_ };
    constraints.add_kind(options_.remote_relation_expression_kinds(), kind);
    return std::move(constraints).release();
}

location_id_set_type element_location::operator()(trelation::expression const& expression) const {
    return collect_locations(options_, expression);
}

location_id_set_type element_location::operator()(tplan::step_kind kind) const {
    location_constraints constraints { query_ };
    constraints.add_kind(options_.remote_step_kinds(), kind);
    return std::move(constraints).release();
}

location_id_set_type element_location::operator()(tplan::step const& step) const {
    return collect_locations(options_, step);
}

location_id_set_type element_location::operator()(tplan::process const& step) const {
    return collect_locations<tplan::step>(options_, step);
}

location_id_set_type element_location::operator()(tplan::exchange const& step) const {
    return collect_locations<tplan::step>(options_, step);
}

location_id_set_type element_location::operator()(tstatement::statement_kind kind) const {
    location_constraints constraints { query_ };
    constraints.add_kind(options_.remote_statement_kinds(), kind);
    return std::move(constraints).release();
}

location_id_set_type element_location::operator()(tstatement::statement const& statement) const {
    return collect_locations(options_, statement);
}

} // namespace yugawara::remote::details
