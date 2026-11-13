#include "element_location.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include <takatori/relation/scan.h>
#include <takatori/relation/find.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/write.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/take_group.h>
#include <takatori/relation/step/take_cogroup.h>

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

#include <takatori/util/downcast.h>

#include <yugawara/binding/extract.h>
#include <yugawara/storage/index.h>

namespace yugawara::details {

namespace descriptor = ::takatori::descriptor;
namespace trelation = ::takatori::relation;
namespace tplan = ::takatori::plan;
namespace tstatement = ::takatori::statement;

using ::takatori::util::unsafe_downcast;

using location_id_set_type = element_location::location_id_set_type;

namespace {

location_id_set_type local_location() {
    return { remote::location_info::local_location_id };
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

} // namespace

element_location::element_location(remote::symbol_location_query const& query) noexcept :
    query_ { query }
{}

location_id_set_type element_location::all() const {
    return query_.available_location_ids();
}

location_id_set_type element_location::operator()(trelation::expression const& expression) const {
    location_intersection result { query_ };
    switch (expression.kind()) {
        case trelation::scan::tag:
            result.add(unsafe_downcast<trelation::scan>(expression).source());
            break;
        case trelation::find::tag:
            result.add(unsafe_downcast<trelation::find>(expression).source());
            break;
        case trelation::join_find::tag:
            result.add(unsafe_downcast<trelation::join_find>(expression).source());
            break;
        case trelation::join_scan::tag:
            result.add(unsafe_downcast<trelation::join_scan>(expression).source());
            break;
        case trelation::write::tag:
            result.add(unsafe_downcast<trelation::write>(expression).destination());
            break;
        case trelation::step::offer::tag:
        case trelation::step::take_flat::tag:
        case trelation::step::take_group::tag:
        case trelation::step::take_cogroup::tag:
            return all();
        default:
            return local_location();
    }
    return std::move(result).release();
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

} // namespace yugawara::details
