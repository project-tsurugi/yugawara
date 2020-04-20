#include "variable_resolution_scanner.h"

#include <yugawara/storage/table.h>

namespace yugawara::serializer::details {

using namespace std::string_view_literals;

using ::takatori::util::optional_ptr;

variable_resolution_scanner::variable_resolution_scanner(
        takatori::serializer::object_scanner& scanner,
        takatori::serializer::object_acceptor& acceptor,
        optional_ptr<analyzer::variable_mapping const> variable_mapping,
        optional_ptr<analyzer::expression_mapping const> expression_mapping) noexcept
    : scanner_(scanner)
    , acceptor_(acceptor)
    , variable_mapping_(std::move(variable_mapping))
    , expression_mapping_(std::move(expression_mapping))
{}

void variable_resolution_scanner::scan(takatori::descriptor::variable const& element) {
    if (variable_mapping_) {
        acceptor_.struct_begin();

        auto&& resolution = variable_mapping_->find(element);

        acceptor_.property_begin("kind"sv);
        acceptor_.string(to_string_view(resolution.kind()));
        acceptor_.property_end();

        analyzer::dispatch(*this, resolution.kind(), resolution);

        acceptor_.struct_end();
    }
}

void variable_resolution_scanner::operator()(tag_t<kind::unresolved>, analyzer::variable_resolution const&) {
    // no special elements
}

void variable_resolution_scanner::operator()(tag_t<kind::unknown>, analyzer::variable_resolution const& element) {
    auto&& entity = element.element<kind::unknown>();

    acceptor_.property_begin("type"sv);
    if (entity) {
        scanner_(*entity, acceptor_);
    }
    acceptor_.property_end();
}

void variable_resolution_scanner::operator()(tag_t<kind::scalar_expression>, analyzer::variable_resolution const& element) {
    auto&& entity = element.element<kind::scalar_expression>();

    acceptor_.property_begin("expression"sv);
    acceptor_.struct_begin();

    acceptor_.property_begin("kind"sv);
    acceptor_.string(to_string_view(entity.kind()));
    acceptor_.property_end();

    acceptor_.property_begin("this"sv);
    acceptor_.pointer(std::addressof(entity));
    acceptor_.property_end();

    acceptor_.struct_end();
    acceptor_.property_end();

    acceptor_.property_begin("type"sv);
    if (expression_mapping_) {
        if (auto v = expression_mapping_->find(entity)) {
            scanner_(*v, acceptor_);
        }
    }
    acceptor_.property_end();
}

void variable_resolution_scanner::operator()(tag_t<kind::table_column>, analyzer::variable_resolution const& element) {
    auto&& entity = element.element<kind::table_column>();

    acceptor_.property_begin("simple_name"sv);
    acceptor_.string(entity.simple_name());
    acceptor_.property_end();

    acceptor_.property_begin("owner"sv);
    if (auto o = entity.optional_owner()) {
        acceptor_.string(o->simple_name());
    }
    acceptor_.property_end();

    acceptor_.property_begin("type"sv);
    if (auto v = entity.optional_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();
}

void variable_resolution_scanner::operator()(tag_t<kind::external>, analyzer::variable_resolution const& element) {
    auto&& entity = element.element<kind::external>();

    acceptor_.property_begin("name"sv);
    acceptor_.string(entity.name());
    acceptor_.property_end();

    acceptor_.property_begin("type"sv);
    if (auto v = entity.optional_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();
}

void variable_resolution_scanner::operator()(tag_t<kind::function_call>, analyzer::variable_resolution const& element) {
    auto&& entity = element.element<kind::function_call>();

    acceptor_.property_begin("definition_id"sv);
    acceptor_.unsigned_integer(entity.definition_id());
    acceptor_.property_end();

    acceptor_.property_begin("name"sv);
    if (auto&& name = entity.name(); !name.empty()) {
        acceptor_.string(entity.name());
    }
    acceptor_.property_end();

    acceptor_.property_begin("type"sv);
    if (auto v = entity.optional_return_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();
}

void variable_resolution_scanner::operator()(tag_t<kind::aggregation>, analyzer::variable_resolution const& element) {
    auto&& entity = element.element<kind::aggregation>();

    acceptor_.property_begin("definition_id"sv);
    acceptor_.unsigned_integer(entity.definition_id());
    acceptor_.property_end();

    acceptor_.property_begin("name"sv);
    if (auto&& name = entity.name(); !name.empty()) {
        acceptor_.string(entity.name());
    }
    acceptor_.property_end();

    acceptor_.property_begin("type"sv);
    if (auto v = entity.optional_return_type()) {
        scanner_(*v, acceptor_);
    }
    acceptor_.property_end();
}

} // namespace yugawara::serializer::details
