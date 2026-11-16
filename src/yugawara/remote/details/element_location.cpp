#include "element_location.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <takatori/relation/filter.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/find.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/write.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/take_group.h>
#include <takatori/relation/step/take_cogroup.h>

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

class location_intersection {
public:
    explicit location_intersection(remote::symbol_location_query const& query) :
        query_ { query },
        result_ { query.available_location_ids() }
    {}

    void add(descriptor::relation const& descriptor) {
        if (auto index = binding::extract_if<storage::index>(descriptor)) {
            add(index->table());
            return;
        }
        intersect(result_, local_location());
    }

    void add(descriptor::storage const& descriptor) {
        if (auto table = binding::extract_if<storage::table>(descriptor)) {
            add(*table);
            return;
        }
        intersect(result_, local_location());
    }

    void add(location_id_set_type const& locations) {
        intersect(result_, locations);
    }

    template<class T>
    void add_privilege_elements(T const& statement) {
        for (auto&& element : statement.elements()) {
            add(element.table());
        }
    }

    [[nodiscard]] location_id_set_type release() && {
        return std::move(result_);
    }

private:
    remote::symbol_location_query const& query_;
    location_id_set_type result_;

    void add(storage::table const& table) {
        intersect(result_, query_.find(table));
    }
};

template<class Keys, class Evaluate>
void add_keys(location_intersection& target, Keys const& keys, Evaluate const& evaluate) {
    for (auto&& key : keys) {
        target.add(evaluate(key.value()));
    }
}

template<class Value>
void add_constant_value(
        location_intersection& target,
        Value const& value,
        remote::symbol_location_query const& query) {
    if (!value.has_variable()) {
        return;
    }
    if (auto variable = binding::extract_if<variable::declaration>(value.variable())) {
        target.add(query.find(*variable));
    }
}

class scalar_location_collector {
public:
    explicit scalar_location_collector(
            remote::location_options::scalar_expression_kind_set const& kinds,
            remote::symbol_location_query const& query,
            location_id_set_type& result) noexcept :
        kinds_ { kinds },
        query_ { query },
        result_ { result }
    {}

    void operator()(tscalar::expression const& expression) {
        check_kind(expression);
    }

    void operator()(tscalar::function_call const& expression) {
        check_kind(expression);
        if (auto function = binding::extract_if<function::declaration>(expression.function())) {
            intersect(result_, query_.find(*function));
            return;
        }
        intersect(result_, local_location());
    }

    void operator()(tscalar::variable_reference const& expression) {
        check_kind(expression);
        if (auto variable = binding::extract_if<variable::declaration>(expression.variable())) {
            intersect(result_, query_.find(*variable));
        }
    }

    void operator()(tscalar::extension const&) {
        // NOTE: the contents of extensions are not inspected
        intersect(result_, local_location());
    }

private:
    remote::location_options::scalar_expression_kind_set const& kinds_;
    remote::symbol_location_query const& query_;
    location_id_set_type& result_;

    void check_kind(tscalar::expression const& expression) {
        if (!kinds_.contains(expression.kind())) {
            intersect(result_, local_location());
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
    auto result = all();
    scalar_location_collector collector { options_.remote_scalar_expression_kinds(), query_, result };
    tscalar::walk(collector, expression);
    return result;
}

location_id_set_type element_location::operator()(trelation::expression const& expression) const {
    bool const remote = options_.remote_relation_expression_kinds().contains(expression.kind());
    location_intersection result { query_ };
    switch (expression.kind()) {
        case trelation::scan::tag: {
            auto&& scan = unsafe_downcast<trelation::scan>(expression);
            result.add(scan.source());
            add_keys(result, scan.lower().keys(), *this);
            add_keys(result, scan.upper().keys(), *this);
            add_constant_value(result, scan.row_slice().start(), query_);
            add_constant_value(result, scan.row_slice().count(), query_);
            break;
        }
        case trelation::find::tag: {
            auto&& find = unsafe_downcast<trelation::find>(expression);
            result.add(find.source());
            add_keys(result, find.keys(), *this);
            break;
        }
        case trelation::join_find::tag:
            result.add(unsafe_downcast<trelation::join_find>(expression).source());
            break;
        case trelation::join_scan::tag:
            result.add(unsafe_downcast<trelation::join_scan>(expression).source());
            break;
        case trelation::write::tag:
            result.add(unsafe_downcast<trelation::write>(expression).destination());
            break;
        // operators which never restrict the location
        case trelation::step::offer::tag:
        case trelation::step::take_flat::tag:
        case trelation::step::take_group::tag:
        case trelation::step::take_cogroup::tag:
            break;
        case trelation::filter::tag:
            result.add((*this)(unsafe_downcast<trelation::filter>(expression).condition()));
            break;
        default:
            return local_location();
    }
    auto locations = std::move(result).release();
    if (!remote) {
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
    location_intersection result { query_ };
    switch (statement.kind()) {
        case tstatement::execute::tag: {
            auto locations = all();
            for (auto&& step : unsafe_downcast<tstatement::execute>(statement).execution_plan()) {
                intersect(locations, (*this)(step));
            }
            return locations;
        }
        case tstatement::write::tag:
            result.add(unsafe_downcast<tstatement::write>(statement).destination());
            break;
        case tstatement::drop_table::tag:
            result.add(unsafe_downcast<tstatement::drop_table>(statement).target());
            break;
        case tstatement::rename_table::tag:
            result.add(unsafe_downcast<tstatement::rename_table>(statement).target());
            break;
        case tstatement::rename_column::tag:
            result.add(unsafe_downcast<tstatement::rename_column>(statement).table());
            break;
        case tstatement::create_index::tag:
            result.add(unsafe_downcast<tstatement::create_index>(statement).definition());
            break;
        case tstatement::drop_index::tag:
            result.add(unsafe_downcast<tstatement::drop_index>(statement).target());
            break;
        case tstatement::rename_index::tag:
            result.add(unsafe_downcast<tstatement::rename_index>(statement).target());
            break;
        case tstatement::truncate_table::tag:
            result.add(unsafe_downcast<tstatement::truncate_table>(statement).target());
            break;
        case tstatement::grant_table::tag:
            result.add_privilege_elements(unsafe_downcast<tstatement::grant_table>(statement));
            break;
        case tstatement::revoke_table::tag:
            result.add_privilege_elements(unsafe_downcast<tstatement::revoke_table>(statement));
            break;
        default:
            return local_location();
    }
    return std::move(result).release();
}

} // namespace yugawara::remote::details
