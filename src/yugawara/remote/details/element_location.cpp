#include "element_location.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <takatori/relation/filter.h>
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

#include <boost/assert.hpp>

#include <yugawara/binding/extract.h>
#include <yugawara/storage/index.h>
#include <yugawara/function/declaration.h>
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

    void add(descriptor::relation const& descriptor) {
        if (auto index = binding::extract_if<storage::index>(descriptor)) {
            add(index->table());
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
        tstatement::dispatch(*this, statement);
    }

    /**
     * @brief collects the constraints of the given step.
     * @param step the target step
     */
    void collect(tplan::step const& step) {
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
        // NOTE: exchanges are always in the local location
        constraints_.restrict_to_local();
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
        // NOTE: the join keys and conditions are not inspected yet
        constraints_.add(expression.source());
        constraints_.restrict_to_local();
    }

    void operator()(trelation::join_scan const& expression) {
        // NOTE: the join keys and conditions are not inspected yet
        constraints_.add(expression.source());
        constraints_.restrict_to_local();
    }

    void operator()(trelation::write const& expression) {
        constraints_.add(expression.destination());
    }

    void operator()(trelation::filter const& expression) {
        tscalar::walk(*this, expression.condition());
    }

    // operators which never have scalar expressions nor symbols
    void operator()(trelation::buffer const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::identify const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::emit const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::intermediate::distinct const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::intermediate::union_ const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::intermediate::intersection const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::intermediate::difference const& expression) noexcept {
        (void) expression;
    }

    void operator()(trelation::intermediate::escape const& expression) noexcept {
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

    void operator()(trelation::step::offer const& expression) noexcept {
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
        if (auto function = binding::extract_if<function::declaration>(expression.function())) {
            constraints_.add(query_.find(*function));
            return;
        }
        constraints_.restrict_to_local();
    }

    void operator()(tscalar::variable_reference const& expression) {
        check_kind(expression);
        if (auto variable = binding::extract_if<variable::declaration>(expression.variable())) {
            constraints_.add(query_.find(*variable));
        }
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

    template<class Keys>
    void add_keys(Keys const& keys) {
        for (auto&& key : keys) {
            tscalar::walk(*this, key.value());
        }
    }

    template<class Value>
    void add_constant_value(Value const& value) {
        if (!value.has_variable()) {
            return;
        }
        if (auto variable = binding::extract_if<variable::declaration>(value.variable())) {
            constraints_.add(query_.find(*variable));
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

location_id_set_type element_location::operator()(tplan::step const& step) const {
    return collect_locations(options_, step);
}

location_id_set_type element_location::operator()(tplan::process const& step) const {
    return collect_locations<tplan::step>(options_, step);
}

location_id_set_type element_location::operator()(tplan::exchange const& step) const {
    return collect_locations<tplan::step>(options_, step);
}

location_id_set_type element_location::operator()(tstatement::statement const& statement) const {
    return collect_locations(options_, statement);
}

} // namespace yugawara::remote::details
