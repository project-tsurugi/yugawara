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

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>
#include <takatori/statement/drop_table.h>
#include <takatori/statement/rename_table.h>
#include <takatori/statement/rename_column.h>
#include <takatori/statement/create_index.h>
#include <takatori/statement/drop_index.h>
#include <takatori/statement/rename_index.h>
#include <takatori/statement/truncate_table.h>
#include <takatori/statement/grant_table.h>
#include <takatori/statement/revoke_table.h>

#include <boost/assert.hpp>

#include <takatori/util/downcast.h>

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

using ::takatori::util::unsafe_downcast;

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
 * @brief decomposes an operator or a scalar expression, and collects its constraints of locations.
 * @details Each operator is restricted by the symbols it refers, and by the scalar expressions in it.
 *      The operators not listed here may have scalar expressions or symbols not inspected here,
 *      so that they are restricted to the local location.
 */
class location_collector {
public:
    explicit location_collector(
            remote::location_options::scalar_expression_kind_set const& scalar_kinds,
            remote::symbol_location_query const& query,
            location_constraints& constraints) noexcept :
        scalar_kinds_ { scalar_kinds },
        query_ { query },
        constraints_ { constraints }
    {}

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
    remote::location_options::scalar_expression_kind_set const& scalar_kinds_;
    remote::symbol_location_query const& query_;
    location_constraints& constraints_;

    void check_kind(tscalar::expression const& expression) {
        if (!scalar_kinds_.contains(expression.kind())) {
            constraints_.restrict_to_local();
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
        if (!value.has_variable()) {
            return;
        }
        if (auto variable = binding::extract_if<variable::declaration>(value.variable())) {
            constraints_.add(query_.find(*variable));
        }
    }
};

} // namespace

element_location::element_location(remote::location_options options) noexcept :
    options_ { std::move(options) },
    query_ { require_query(options_) }
{}

location_id_set_type element_location::all() const {
    return query_.available_location_ids();
}

location_id_set_type element_location::operator()(tscalar::expression const& expression) const {
    location_constraints constraints { query_ };
    location_collector collector { options_.remote_scalar_expression_kinds(), query_, constraints };
    tscalar::walk(collector, expression);
    return std::move(constraints).release();
}

location_id_set_type element_location::operator()(trelation::expression const& expression) const {
    location_constraints constraints { query_ };
    location_collector collector { options_.remote_scalar_expression_kinds(), query_, constraints };
    if (trelation::is_available_in_intermediate_plan(expression.kind())) {
        trelation::intermediate::dispatch(collector, expression);
    } else {
        trelation::step::dispatch(collector, expression);
    }
    auto locations = std::move(constraints).release();
    if (!options_.remote_relation_expression_kinds().contains(expression.kind())) {
        intersect(locations, local_location());
    }
    return locations;
}

location_id_set_type element_location::operator()(tplan::step const& step) const {
    if (step.kind() == tplan::process::tag) {
        return (*this)(unsafe_downcast<tplan::process>(step));
    }
    return (*this)(unsafe_downcast<tplan::exchange>(step));
}

location_id_set_type element_location::operator()(tplan::process const& step) const {
    auto result = all();
    for (auto&& expression : step.operators()) {
        intersect(result, (*this)(expression));
    }
    return result;
}

location_id_set_type element_location::operator()(tplan::exchange const&) const {
    return local_location();
}

location_id_set_type element_location::operator()(tstatement::statement const& statement) const {
    location_constraints constraints { query_ };
    switch (statement.kind()) {
        case tstatement::execute::tag: {
            auto locations = all();
            for (auto&& step : unsafe_downcast<tstatement::execute>(statement).execution_plan()) {
                intersect(locations, (*this)(step));
            }
            return locations;
        }
        case tstatement::write::tag: {
            auto&& write = unsafe_downcast<tstatement::write>(statement);
            constraints.add(write.destination());
            for (auto&& tuple : write.tuples()) {
                for (auto&& element : tuple.elements()) {
                    constraints.add((*this)(element));
                }
            }
            break;
        }
        case tstatement::drop_table::tag:
            constraints.add(unsafe_downcast<tstatement::drop_table>(statement).target());
            break;
        case tstatement::rename_table::tag:
            constraints.add(unsafe_downcast<tstatement::rename_table>(statement).target());
            break;
        case tstatement::rename_column::tag:
            constraints.add(unsafe_downcast<tstatement::rename_column>(statement).table());
            break;
        case tstatement::create_index::tag:
            constraints.add(unsafe_downcast<tstatement::create_index>(statement).definition());
            break;
        case tstatement::drop_index::tag:
            constraints.add(unsafe_downcast<tstatement::drop_index>(statement).target());
            break;
        case tstatement::rename_index::tag:
            constraints.add(unsafe_downcast<tstatement::rename_index>(statement).target());
            break;
        case tstatement::truncate_table::tag:
            constraints.add(unsafe_downcast<tstatement::truncate_table>(statement).target());
            break;
        case tstatement::grant_table::tag:
            constraints.add_privilege_elements(unsafe_downcast<tstatement::grant_table>(statement));
            break;
        case tstatement::revoke_table::tag:
            constraints.add_privilege_elements(unsafe_downcast<tstatement::revoke_table>(statement));
            break;
        default:
            return local_location();
    }
    return std::move(constraints).release();
}

} // namespace yugawara::remote::details
