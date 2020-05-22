#include "binding_scanner.h"

#include <takatori/util/downcast.h>

#include "predicate_scanner.h"

namespace yugawara::serializer::details {

using namespace std::string_view_literals;

using ::takatori::util::unsafe_downcast;
using ::takatori::util::optional_ptr;

binding_scanner::binding_scanner(
        takatori::serializer::object_scanner const& scanner,
        takatori::serializer::object_acceptor& acceptor,
        optional_ptr<analyzer::variable_mapping const> variable_mapping,
        optional_ptr<analyzer::expression_mapping const> expression_mapping) noexcept
    : scanner_(scanner)
    , acceptor_(acceptor)
    , variable_mapping_(std::move(variable_mapping))
    , expression_mapping_(std::move(expression_mapping))
{}

void binding_scanner::scan(binding::variable_info const& element) {
    acceptor_.struct_begin();

    acceptor_.property_begin("kind"sv);
    acceptor_.string(to_string_view(element.kind()));
    acceptor_.property_end();

    using k = binding::variable_info_kind;
    switch (element.kind()) {
        case k::table_column: properties(unsafe_downcast<binding::table_column_info>(element)); break;
        case k::exchange_column: properties(unsafe_downcast<binding::variable_info_impl<k::exchange_column>>(element)); break;
        case k::frame_variable: properties(unsafe_downcast<binding::variable_info_impl<k::frame_variable>>(element)); break;
        case k::stream_variable: properties(unsafe_downcast<binding::variable_info_impl<k::stream_variable>>(element)); break;
        case k::local_variable: properties(unsafe_downcast<binding::variable_info_impl<k::local_variable>>(element)); break;
        case k::external_variable: properties(unsafe_downcast<binding::external_variable_info>(element)); break;
    }

    acceptor_.struct_end();
}

void binding_scanner::scan(binding::relation_info const& element) {
    acceptor_.struct_begin();

    acceptor_.property_begin("kind"sv);
    acceptor_.string(to_string_view(element.kind()));
    acceptor_.property_end();

    using k = binding::relation_info_kind;
    switch (element.kind()) {
        case k::index: properties(unsafe_downcast<binding::index_info>(element)); break;
        case k::exchange: properties(unsafe_downcast<binding::exchange_info>(element)); break;
    }

    acceptor_.struct_end();
}

void binding_scanner::scan(binding::function_info const& element) {
    acceptor_.struct_begin();
    properties(element.declaration());
    acceptor_.struct_end();
}

void binding_scanner::scan(binding::aggregate_function_info const& element) {
    acceptor_.struct_begin();
    properties(element.declaration());
    acceptor_.struct_end();
}

void binding_scanner::properties(storage::column const& element) {
    acceptor_.property_begin("simple_name"sv);
    acceptor_.string(element.simple_name());
    acceptor_.property_end();

    acceptor_.property_begin("type"sv);
    if (auto v = element.optional_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();

    acceptor_.property_begin("criteria"sv);
    accept(element.criteria());
    acceptor_.property_end();

    acceptor_.property_begin("default_value"sv);
    if (auto v = element.default_value()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();

    acceptor_.property_begin("owner"sv);
    if (auto owner = element.optional_owner()) {
        acceptor_.struct_begin();

        acceptor_.property_begin("kind"sv);
        acceptor_.string(to_string_view(owner->kind()));
        acceptor_.property_end();

        acceptor_.property_begin("simple_name"sv);
        acceptor_.string(element.simple_name());
        acceptor_.property_end();

        acceptor_.struct_end();

    }
    acceptor_.property_end();
}

void binding_scanner::properties(storage::index const& element) {
    acceptor_.property_begin("table"sv);
    acceptor_.string(element.table().simple_name());
    acceptor_.property_end();

    acceptor_.property_begin("simple_name"sv);
    acceptor_.string(element.simple_name());
    acceptor_.property_end();

    acceptor_.property_begin("keys"sv);
    acceptor_.array_begin();
    for (auto&& key : element.keys()) {
        accept(key);
    }
    acceptor_.array_end();
    acceptor_.property_end();

    acceptor_.property_begin("values"sv);
    acceptor_.array_begin();
    for (auto&& value : element.values()) {
        acceptor_.string(value.get().simple_name());
    }
    acceptor_.array_end();
    acceptor_.property_end();

    acceptor_.property_begin("features"sv);
    acceptor_.array_begin();
    for (auto&& feature : element.features()) {
        acceptor_.string(to_string_view(feature));
    }
    acceptor_.array_end();
    acceptor_.property_end();
}

void binding_scanner::properties(takatori::plan::exchange const& element) {
    acceptor_.property_begin("this"sv);
    acceptor_.pointer(std::addressof(element));
    acceptor_.property_end();
}

void binding_scanner::properties(variable::declaration const& element) {
    acceptor_.property_begin("name"sv);
    acceptor_.string(element.name());
    acceptor_.property_end();

    acceptor_.property_begin("type"sv);
    if (auto v = element.optional_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();

    acceptor_.property_begin("criteria"sv);
    accept(element.criteria());
    acceptor_.property_end();
}

void binding_scanner::properties(function::declaration const& element) {
    acceptor_.property_begin("definition_id"sv);
    acceptor_.unsigned_integer(element.definition_id());
    acceptor_.property_end();

    acceptor_.property_begin("name"sv);
    if (auto&& n = element.name(); !n.empty()) {
        acceptor_.string(n);
    }
    acceptor_.property_end();

    acceptor_.property_begin("return_type"sv);
    if (auto v = element.optional_return_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();

    acceptor_.property_begin("parameter_types"sv);
    acceptor_.array_begin();
    for (auto&& v : element.parameter_types()) {
        scanner_(v, acceptor_);
    }
    acceptor_.array_end();
    acceptor_.property_end();
}

void binding_scanner::properties(aggregate::declaration const& element) {
    acceptor_.property_begin("definition_id"sv);
    acceptor_.unsigned_integer(element.definition_id());
    acceptor_.property_end();

    acceptor_.property_begin("name"sv);
    if (auto&& n = element.name(); !n.empty()) {
        acceptor_.string(element.name());
    }
    acceptor_.property_end();

    acceptor_.property_begin("quantifier"sv);
    if (element) {
        acceptor_.string(to_string_view(element.quantifier()));
    }
    acceptor_.property_end();

    acceptor_.property_begin("return_type"sv);
    if (auto v = element.optional_return_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();

    acceptor_.property_begin("parameter_types"sv);
    acceptor_.array_begin();
    for (auto&& v : element.parameter_types()) {
        scanner_(v, acceptor_);
    }
    acceptor_.array_end();
    acceptor_.property_end();

    acceptor_.property_begin("incremental"sv);
    if (element) {
        acceptor_.boolean(element.incremental());
    }
    acceptor_.property_end();
}

void binding_scanner::accept(variable::criteria const& element) {
    acceptor_.struct_begin();

    acceptor_.property_begin("nullity"sv);
    acceptor_.string(to_string_view(element.nullity()));
    acceptor_.property_end();

    acceptor_.property_begin("predicate"sv);
    if (auto v = element.predicate()) {
        predicate_scanner s { scanner_, acceptor_ };
        s.scan(*v);
    }
    acceptor_.property_end();

    acceptor_.struct_end();
}

void binding_scanner::accept(storage::details::index_key_element const& element) {
    acceptor_.struct_begin();

    acceptor_.property_begin("column"sv);
    acceptor_.string(element.column().simple_name());
    acceptor_.property_end();

    acceptor_.property_begin("direction"sv);
    acceptor_.string(to_string_view(element.direction()));
    acceptor_.property_end();

    acceptor_.struct_end();
}

void binding_scanner::properties(binding::table_column_info const& element) {
    properties(element.column());
}

void binding_scanner::properties(binding::external_variable_info const& element) {
    properties(element.declaration());
}

template<binding::variable_info_kind Kind>
void binding_scanner::properties(binding::variable_info_impl<Kind> const& element) {
    acceptor_.property_begin("this"sv);
    acceptor_.pointer(std::addressof(element));
    acceptor_.property_end();

    acceptor_.property_begin("label"sv);
    if (!element.label().empty()) {
        acceptor_.string(element.label());
    }
    acceptor_.property_end();
}

void binding_scanner::properties(binding::index_info const& element) {
    properties(element.declaration());
}

void binding_scanner::properties(binding::exchange_info const& element) {
    properties(element.declaration());
}

} // namespace yugawara::serializer::details
