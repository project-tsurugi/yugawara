#include <yugawara/serializer/object_scanner.h>

#include <takatori/type/extension.h>
#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/variable_info.h>
#include <yugawara/binding/relation_info.h>
#include <yugawara/binding/function_info.h>
#include <yugawara/binding/aggregate_function_info.h>

#include <yugawara/extension/type/extension_id.h>
#include <yugawara/extension/scalar/aggregate_function_call.h>

#include "details/binding_scanner.h"
#include "details/resolution_scanner.h"
#include "details/extension_type_property_scanner.h"
#include "details/extension_scalar_property_scanner.h"

namespace yugawara::serializer {

using namespace std::string_view_literals;

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
using ::takatori::serializer::object_acceptor;
using ::takatori::util::maybe_shared_ptr;
using ::takatori::util::unsafe_downcast;
using ::takatori::util::optional_ptr; // NOLINT(misc-unused-using-decls)

object_scanner::object_scanner(
        maybe_shared_ptr<analyzer::variable_mapping const> variable_mapping,
        maybe_shared_ptr<analyzer::expression_mapping const> expression_mapping) noexcept
    : ::takatori::serializer::object_scanner(true)
    , variable_mapping_(std::move(variable_mapping))
    , expression_mapping_(std::move(expression_mapping))
{}

object_scanner& object_scanner::variable_mapping(maybe_shared_ptr<analyzer::variable_mapping const> mapping) noexcept {
    variable_mapping_ = std::move(mapping);
    return *this;
}

object_scanner& object_scanner::expression_mapping(maybe_shared_ptr<analyzer::expression_mapping const> mapping) noexcept {
    expression_mapping_ = std::move(mapping);
    return *this;
}

void object_scanner::properties(descriptor::variable const& element, object_acceptor& acceptor) const {
    accept_properties(element, acceptor);
    accept_resolution(element, acceptor);
}

void object_scanner::properties(descriptor::relation const& element, object_acceptor& acceptor) const {
    accept_properties(element, acceptor);
}

void object_scanner::properties(descriptor::function const& element, object_acceptor& acceptor) const {
    accept_properties(element, acceptor);
}

void object_scanner::properties(descriptor::aggregate_function const& element, object_acceptor& acceptor) const {
    accept_properties(element, acceptor);
}

void object_scanner::properties(takatori::type::data const& element, object_acceptor& acceptor) const {
    ::takatori::serializer::object_scanner::properties(element, acceptor);
    if (element.kind() == ::takatori::type::extension::tag) {
        auto&& ext = unsafe_downcast<::takatori::type::extension>(element);
        if (extension::type::is_known_compiler_extension(ext.extension_id())) {
            details::extension_type_property_scanner s { *this, acceptor };
            s.process(ext);
            return;
        }
    }
}

void object_scanner::properties(scalar::expression const& element, object_acceptor& acceptor) const {
    acceptor.property_begin("this"sv);
    acceptor.pointer(std::addressof(element));
    acceptor.property_end();

    if (element.kind() == ::takatori::scalar::extension::tag) {
        auto&& ext = unsafe_downcast<::takatori::scalar::extension>(element);
        if (extension::scalar::is_known_compiler_extension(ext.extension_id())) {
            details::extension_scalar_property_scanner s { *this, acceptor };
            s.process(ext);
            return;
        }
    }

    ::takatori::serializer::object_scanner::properties(element, acceptor);
    accept_resolution(element, acceptor);
}

template<::takatori::descriptor::descriptor_kind Kind>
void object_scanner::accept_properties(descriptor::element<Kind> const& element, object_acceptor& acceptor) const {
    auto&& info = binding::unwrap(element);
    acceptor.property_begin("binding"sv);
    details::binding_scanner s {
            *this,
            acceptor,
            optional_ptr { variable_mapping_.get() },
            optional_ptr { expression_mapping_.get() },
    };
    s.scan(info);
    acceptor.property_end();
}

template<class T>
void object_scanner::accept_resolution(T const& element, object_acceptor& acceptor) const {
    acceptor.property_begin("resolution"sv);
    details::resolution_scanner s {
            *this,
            acceptor,
            optional_ptr { variable_mapping_.get() },
            optional_ptr { expression_mapping_.get() },
    };
    s.scan(element);
    acceptor.property_end();
}

} // namespace yugawara::serializer
