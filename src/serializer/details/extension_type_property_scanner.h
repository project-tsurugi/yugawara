#pragma once

#include <takatori/type/extension.h>

#include <takatori/serializer/object_acceptor.h>
#include <takatori/serializer/object_scanner.h>

namespace yugawara::serializer::details {

class extension_type_property_scanner {
public:
    explicit extension_type_property_scanner(
            ::takatori::serializer::object_scanner const& scanner,
            ::takatori::serializer::object_acceptor& acceptor) noexcept;

    void process(::takatori::type::extension const& element);

private:
    ::takatori::serializer::object_acceptor& acceptor_;
};

} // namespace yugawara::serializer::details
