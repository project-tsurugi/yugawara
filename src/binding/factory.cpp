#include <yugawara/binding/factory.h>

#include <takatori/util/clonable.h>

#include "variable_info_impl.h"

#include <yugawara/binding/table_column_info.h>

namespace yugawara::binding {

factory::factory(::takatori::util::object_creator creator) noexcept
    : creator_(creator)
{}

::takatori::descriptor::relation factory::index(storage::index const& declaration) {
    return wrap(creator_.create_shared<index_info>(declaration));
}

::takatori::descriptor::relation factory::exchange(::takatori::plan::exchange const& declaration) {
    return wrap(creator_.create_shared<exchange_info>(declaration));
}

::takatori::descriptor::variable factory::table_column(storage::column const& column) {
    return wrap(creator_.create_shared<table_column_info>(column));
}

factory::variable_vector factory::table_columns(storage::column_list_view const& columns) {
    variable_vector vars { creator_.allocator<takatori::descriptor::variable>() };
    vars.reserve(columns.size());
    for (auto&& c : columns) {
        vars.emplace_back(table_column(c));
    }
    return vars;
}

template<variable_info_kind Kind>
static takatori::descriptor::variable create(::takatori::util::object_creator creator, std::string label) {
    using info = variable_info_impl<Kind>;
    return wrap(creator.create_shared<info>(std::move(label)));
}

takatori::descriptor::variable factory::exchange_column(std::string label) {
    return create<variable_info_kind::exchange_column>(creator_, std::move(label));
}

::takatori::descriptor::variable factory::external_variable(variable::declaration const& declaration) {
    return wrap(creator_.create_shared<external_variable_info>(std::move(declaration)));
}

::takatori::descriptor::variable factory::stream_variable(std::string label) {
    return create<variable_info_kind::stream_variable>(creator_, std::move(label));
}

::takatori::descriptor::variable factory::local_variable(std::string label) {
    return create<variable_info_kind::local_variable>(creator_, std::move(label));
}

::takatori::descriptor::function factory::function(std::shared_ptr<function::declaration const> declaration) {
    return wrap(creator_.create_shared<function_info>(std::move(declaration)));
}

::takatori::descriptor::function factory::function(function::declaration&& declaration) {
    return function(creator_.create_shared<function::declaration>(std::move(declaration)));
}

::takatori::descriptor::aggregate_function factory::aggregate_function(
        std::shared_ptr<aggregate::declaration const> declaration) {
    return wrap(creator_.create_shared<aggregate_function_info>(std::move(declaration)));
}

::takatori::descriptor::aggregate_function factory::aggregate_function(aggregate::declaration&& declaration) {
    return aggregate_function(creator_.create_shared<aggregate::declaration>(std::move(declaration)));
}

} // namespace yugawara::binding
