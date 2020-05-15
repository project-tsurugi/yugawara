#include "extension_type_property_scanner.h"

#include <string_view>

#include <yugawara/extension/type/extension_id.h>

namespace yugawara::serializer::details {

using namespace std::string_view_literals;

details::extension_type_property_scanner::extension_type_property_scanner(
        ::takatori::serializer::object_scanner&,
        ::takatori::serializer::object_acceptor& acceptor) noexcept
    : acceptor_(acceptor)
{}

void extension_type_property_scanner::process(takatori::type::extension const& element) {
    acceptor_.property_begin("extension_tag"sv);
    acceptor_.string(to_string_view(static_cast<extension::type::extension_id>(element.extension_id())));
    acceptor_.property_end();
}

} // namespace yugawara::serializer::details
