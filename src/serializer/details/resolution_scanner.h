#pragma once

#include <takatori/serializer/object_acceptor.h>
#include <takatori/serializer/object_scanner.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/analyzer/expression_mapping.h>
#include <yugawara/analyzer/variable_mapping.h>

namespace yugawara::serializer::details {

class resolution_scanner {
public:
    template<auto K>
    using tag_t = ::takatori::util::enum_tag_t<K>;
    using kind = analyzer::variable_resolution_kind;

    explicit resolution_scanner(
            ::takatori::serializer::object_scanner const& scanner,
            ::takatori::serializer::object_acceptor& acceptor,
            ::takatori::util::optional_ptr<analyzer::variable_mapping const> variable_mapping,
            ::takatori::util::optional_ptr<analyzer::expression_mapping const> expression_mapping) noexcept;

    void scan(::takatori::descriptor::variable const& element);
    void scan(::takatori::scalar::expression const& element);

    void operator()(tag_t<kind::unresolved>, analyzer::variable_resolution const& element);
    void operator()(tag_t<kind::unknown>, analyzer::variable_resolution const& element);
    void operator()(tag_t<kind::scalar_expression>, analyzer::variable_resolution const& element);
    void operator()(tag_t<kind::table_column>, analyzer::variable_resolution const& element);
    void operator()(tag_t<kind::external>, analyzer::variable_resolution const& element);
    void operator()(tag_t<kind::function_call>, analyzer::variable_resolution const& element);
    void operator()(tag_t<kind::aggregation>, analyzer::variable_resolution const& element);

private:
    ::takatori::serializer::object_scanner const& scanner_;
    ::takatori::serializer::object_acceptor& acceptor_;
    ::takatori::util::optional_ptr<analyzer::variable_mapping const> variable_mapping_;
    ::takatori::util::optional_ptr<analyzer::expression_mapping const> expression_mapping_;
};

} // namespace yugawara::serializer::details
