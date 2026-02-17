#include "extension_relation_property_scanner.h"

#include <string_view>

#include <takatori/util/downcast.h>

#include <yugawara/extension/relation/extension_id.h>

namespace yugawara::serializer::details {

using namespace std::string_view_literals;

using ::takatori::util::unsafe_downcast;

extension_relation_property_scanner::extension_relation_property_scanner(
        takatori::serializer::object_scanner const& scanner,
        takatori::serializer::object_acceptor& acceptor) noexcept :
    scanner_ { scanner },
    acceptor_ { acceptor }
{}

void extension_relation_property_scanner::process(::takatori::relation::intermediate::extension const& element) {
    acceptor_.property_begin("extension_tag"sv);
    acceptor_.string(to_string_view(static_cast<extension::relation::extension_id>(element.extension_id())));
    acceptor_.property_end();

    using namespace extension::relation;

    switch (element.extension_id()) {
        case subquery::extension_tag:
            properties(unsafe_downcast<subquery>(element));
            break;
        default:
            // no more information
            break;
    }
}

void extension_relation_property_scanner::properties(extension::relation::subquery const& element) {
    acceptor_.property_begin("query_graph"sv);
    accept(element.query_graph());
    acceptor_.property_end();

    acceptor_.property_begin("mappings"sv);
    acceptor_.array_begin();
    for (auto&& mapping : element.mappings()) {
        acceptor_.struct_begin();

        acceptor_.property_begin("source"sv);
        accept(mapping.source());
        acceptor_.property_end();

        acceptor_.property_begin("destination"sv);
        accept(mapping.destination());
        acceptor_.property_end();

        acceptor_.struct_end();
    }
    acceptor_.array_end();
    acceptor_.property_end();

    acceptor_.property_begin("is_clone"sv);
    acceptor_.boolean(element.is_clone());
    acceptor_.property_end();
}

template <class T>
void extension_relation_property_scanner::accept(::takatori::util::optional_ptr<T const> element) {
    if (element) {
        scanner_(*element, acceptor_);
    }
}

template <class T>
void extension_relation_property_scanner::accept(T const& element) {
    if constexpr (std::is_enum_v<T>) { // NOLINT
        acceptor_.string(to_string_view(element));
    } else { // NOLINT
        scanner_(element, acceptor_);
    }
}

} // namespace yugawara::serializer::details
