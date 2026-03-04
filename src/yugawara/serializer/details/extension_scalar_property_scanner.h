#pragma once

#include <takatori/scalar/extension.h>

#include <takatori/serializer/object_acceptor.h>
#include <takatori/serializer/object_scanner.h>

#include <yugawara/extension/scalar/aggregate_function_call.h>
#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/scalar/exists.h>
#include <yugawara/extension/scalar/quantified_compare.h>

namespace yugawara::serializer::details {

class extension_scalar_property_scanner {
public:
    explicit extension_scalar_property_scanner(
            ::takatori::serializer::object_scanner const& scanner,
            ::takatori::serializer::object_acceptor& acceptor) noexcept;

    void process(::takatori::scalar::extension const& element);

private:
    ::takatori::serializer::object_scanner const& scanner_;
    ::takatori::serializer::object_acceptor& acceptor_;

    void properties(extension::scalar::aggregate_function_call const& element);

    void properties(extension::scalar::subquery const& element);

    void properties(extension::scalar::exists const& element);

    void properties(extension::scalar::quantified_compare const& element);

    template<class T>
    void accept(::takatori::util::optional_ptr<T const> element);

    template<class T>
    void accept(T const& element);
};

} // namespace yugawara::serializer::details
