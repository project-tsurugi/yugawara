#pragma once

#include <takatori/serializer/object_acceptor.h>
#include <takatori/serializer/object_scanner.h>

#include <takatori/plan/exchange.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/relation_info.h>
#include <yugawara/binding/function_info.h>
#include <yugawara/binding/aggregate_function_info.h>

#include <yugawara/binding/table_column_info.h>
#include <yugawara/binding/external_variable_info.h>
#include <binding/variable_info_impl.h>

#include <yugawara/binding/index_info.h>
#include <yugawara/binding/exchange_info.h>

#include <yugawara/storage/index.h>
#include <yugawara/storage/column.h>
#include <yugawara/function/declaration.h>
#include <yugawara/aggregate/declaration.h>
#include <yugawara/variable/declaration.h>

#include <yugawara/analyzer/expression_mapping.h>
#include <yugawara/analyzer/variable_mapping.h>

namespace yugawara::serializer::details {

class binding_scanner {
public:
    explicit binding_scanner(
            ::takatori::serializer::object_scanner const& scanner,
            ::takatori::serializer::object_acceptor& acceptor,
            ::takatori::util::optional_ptr<analyzer::variable_mapping const> variable_mapping,
            ::takatori::util::optional_ptr<analyzer::expression_mapping const> expression_mapping) noexcept;

    void scan(binding::variable_info const& element);
    void scan(binding::relation_info const& element);
    void scan(binding::function_info const& element);
    void scan(binding::aggregate_function_info const& element);

    void properties(storage::column const& element);
    void properties(storage::index const& element);
    void properties(::takatori::plan::exchange const& element);
    void properties(variable::declaration const& element);
    void properties(function::declaration const& element);
    void properties(aggregate::declaration const& element);

private:
    ::takatori::serializer::object_scanner const& scanner_;
    ::takatori::serializer::object_acceptor& acceptor_;
    ::takatori::util::optional_ptr<analyzer::variable_mapping const> variable_mapping_;
    ::takatori::util::optional_ptr<analyzer::expression_mapping const> expression_mapping_;

    void accept(variable::criteria const& element);
    void accept(storage::details::index_key_element const& element);

    void properties(binding::table_column_info const& element);
    void properties(binding::external_variable_info const& element);

    template<binding::variable_info_kind Kind>
    void properties(binding::variable_info_impl<Kind> const& element);

    void properties(binding::index_info const& element);
    void properties(binding::exchange_info const& element);
};

} // namespace yugawara::serializer::details
