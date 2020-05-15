#include "extension_scalar_property_scanner.h"

#include <string_view>

#include <takatori/util/downcast.h>

#include <yugawara/extension/scalar/extension_id.h>

namespace yugawara::serializer::details {

using namespace std::string_view_literals;

using ::takatori::util::unsafe_downcast;

details::extension_scalar_property_scanner::extension_scalar_property_scanner(
        ::takatori::serializer::object_scanner& scanner,
        ::takatori::serializer::object_acceptor& acceptor) noexcept
    : scanner_(scanner)
    , acceptor_(acceptor)
{}

void extension_scalar_property_scanner::process(takatori::scalar::extension const& element) {
    acceptor_.property_begin("extension_tag"sv);
    acceptor_.string(to_string_view(static_cast<extension::scalar::extension_id>(element.extension_id())));
    acceptor_.property_end();

    using namespace extension::scalar;

    if (element.extension_id() == aggregate_function_call::extension_tag) {
        properties(unsafe_downcast<aggregate_function_call>(element));
        return;
    }
}

void extension_scalar_property_scanner::properties(extension::scalar::aggregate_function_call const& element) {
    acceptor_.property_begin("function"sv);
    accept(element.function());
    acceptor_.property_end();

    acceptor_.property_begin("arguments"sv);
    acceptor_.array_begin();
    for (auto&& argument : element.arguments()) {
        accept(argument);
    }
    acceptor_.array_end();
    acceptor_.property_end();
}

template<class T>
void extension_scalar_property_scanner::accept(::takatori::util::optional_ptr<T const> element) {
    if (element) {
        scanner_(*element, acceptor_);
    }
}

template<class T>
void extension_scalar_property_scanner::accept(T const& element) {
    if constexpr (std::is_enum_v<T>) { // NOLINT
        acceptor_.string(to_string_view(element));
    } else { // NOLINT
        scanner_(element, acceptor_);
    }
}

} // namespace yugawara::serializer::details
