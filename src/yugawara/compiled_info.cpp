#include <yugawara/compiled_info.h>

#include <takatori/util/exception.h>
#include <takatori/util/fail.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/extract.h>

namespace yugawara {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;

using ::takatori::util::fail;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unique_object_ptr;

yugawara::compiled_info::compiled_info(
        std::shared_ptr<analyzer::expression_mapping const> expression_mapping,
        std::shared_ptr<analyzer::variable_mapping const> variable_mapping) noexcept :
    expression_mapping_ { std::move(expression_mapping) },
    variable_mapping_ { std::move(variable_mapping) }
{}

::takatori::type::data const& compiled_info::type_of(scalar::expression const& expression) const {
    auto&& resolution = expression_mapping_->find(expression);
    if (resolution) {
        return resolution.type();
    }
    throw_exception(string_builder {}
            << "expression type is not yet resolved: "
            << expression
            << string_builder::to_string);
}

::takatori::type::data const& compiled_info::type_of(descriptor::variable const& variable) const {
    auto&& resolution = variable_mapping_->find(variable);
    using kind = analyzer::variable_resolution_kind;
    switch (resolution.kind()) {
        case kind::unknown: return *resolution.element<kind::unknown>();
        case kind::scalar_expression: return type_of(resolution.element<kind::scalar_expression>());
        case kind::table_column: return resolution.element<kind::table_column>().type();
        case kind::external: return resolution.element<kind::external>().type();
        case kind::function_call: return resolution.element<kind::function_call>().return_type();
        case kind::aggregation: return resolution.element<kind::aggregation>().return_type();
        case kind::unresolved: throw_exception(string_builder {}
                    << "variable type is not yet resolved: "
                    << variable
                    << string_builder::to_string);
    }
    fail();
}

serializer::object_scanner compiled_info::object_scanner() const noexcept {
    return serializer::object_scanner {
            variable_mapping_,
            expression_mapping_,
    };
}

} // namespace yugawara
