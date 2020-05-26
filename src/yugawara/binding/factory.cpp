#include <yugawara/binding/factory.h>

#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/function_info.h>
#include <yugawara/binding/aggregate_function_info.h>
#include <yugawara/binding/relation_info.h>

#include <yugawara/binding/table_column_info.h>
#include <yugawara/binding/external_variable_info.h>

#include <yugawara/binding/index_info.h>
#include <yugawara/binding/exchange_info.h>

#include "variable_info_impl.h"

namespace yugawara::binding {

::takatori::descriptor::relation factory::index(storage::index const& declaration) {
    return wrap(creator_.create_shared<index_info>(declaration));
}

::takatori::descriptor::relation factory::exchange(::takatori::plan::exchange const& declaration) {
    return wrap(creator_.create_shared<exchange_info>(declaration));
}

::takatori::descriptor::variable factory::table_column(storage::column const& declaration) {
    return wrap(creator_.create_shared<table_column_info>(declaration));
}

factory::variable_vector factory::table_columns(storage::column_list_view const& columns) {
    variable_vector vars { creator_.allocator() };
    vars.reserve(columns.size());
    for (auto&& c : columns) {
        vars.emplace_back(table_column(c));
    }
    return vars;
}

template<variable_info_kind Kind>
static takatori::descriptor::variable create(::takatori::util::object_creator creator, std::string_view label) {
    using info = variable_info_impl<Kind>;
    return wrap(creator.create_shared<info>(std::string { label }));
}

takatori::descriptor::variable factory::exchange_column(std::string_view label) {
    return create<variable_info_kind::exchange_column>(creator_, label);
}

::takatori::descriptor::variable factory::external_variable(std::shared_ptr<variable::declaration const> declaration) {
    return wrap(creator_.create_shared<external_variable_info>(std::move(declaration)));
}

::takatori::descriptor::variable factory::external_variable(variable::declaration&& declaration) {
    return external_variable(creator_.create_shared<variable::declaration>(std::move(declaration)));
}

::takatori::descriptor::variable factory::frame_variable(std::string_view label) {
    return create<variable_info_kind::frame_variable>(creator_, label);
}

::takatori::descriptor::variable factory::stream_variable(std::string_view label) {
    return create<variable_info_kind::stream_variable>(creator_, label);
}

::takatori::descriptor::variable factory::local_variable(std::string_view label) {
    return create<variable_info_kind::local_variable>(creator_, label);
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

::takatori::descriptor::relation factory::operator()(storage::index const& declaration) {
    return index(declaration);
}

::takatori::descriptor::relation factory::operator()(::takatori::plan::exchange const& declaration) {
    return exchange(declaration);
}

::takatori::descriptor::variable factory::operator()(storage::column const& declaration) {
    return table_column(declaration);
}

::takatori::descriptor::variable factory::operator()(std::shared_ptr<variable::declaration const> declaration) {
    return external_variable(std::move(declaration));
}

::takatori::descriptor::variable factory::operator()(variable::declaration&& declaration) {
    return external_variable(std::move(declaration));
}

::takatori::descriptor::function factory::operator()(std::shared_ptr<function::declaration const> declaration) {
    return function(std::move(declaration));
}

::takatori::descriptor::function factory::operator()(function::declaration&& declaration) {
    return function(std::move(declaration));
}

::takatori::descriptor::aggregate_function factory::operator()(
        std::shared_ptr<aggregate::declaration const> declaration) {
    return aggregate_function(std::move(declaration));
}

::takatori::descriptor::aggregate_function factory::operator()(aggregate::declaration&& declaration) {
    return aggregate_function(std::move(declaration));
}

} // namespace yugawara::binding
