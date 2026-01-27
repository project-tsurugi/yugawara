#pragma once

#include <takatori/relation/intermediate/extension.h>

#include <takatori/serializer/object_acceptor.h>
#include <takatori/serializer/object_scanner.h>

#include <yugawara/extension/relation/subquery.h>

namespace yugawara::serializer::details {

class extension_relation_property_scanner {
public:
    explicit extension_relation_property_scanner(
            ::takatori::serializer::object_scanner const& scanner,
            ::takatori::serializer::object_acceptor& acceptor) noexcept;

    void process(::takatori::relation::intermediate::extension const& element);

private:
    ::takatori::serializer::object_scanner const& scanner_;
    ::takatori::serializer::object_acceptor& acceptor_;

    void properties(extension::relation::subquery const& element);

    template<class T>
    void accept(::takatori::util::optional_ptr<T const> element);

    template<class T>
    void accept(T const& element);
};

} // namespace yugawara::serializer::details
