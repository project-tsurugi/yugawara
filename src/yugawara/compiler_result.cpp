#include <yugawara/compiler_result.h>

#include <takatori/util/exception.h>
#include <takatori/util/fail.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/extract.h>

namespace yugawara {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace statement = ::takatori::statement;

using diagnostic_type = compiler_result::diagnostic_type;

using ::takatori::util::fail;
using ::takatori::util::optional_ptr;
using ::takatori::util::sequence_view;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unique_object_ptr;

yugawara::compiler_result::compiler_result(
        unique_object_ptr<statement::statement> statement,
        std::shared_ptr<analyzer::expression_mapping const> expression_mapping,
        std::shared_ptr<analyzer::variable_mapping const> variable_mapping) noexcept
    : statement_(std::move(statement))
    , expression_mapping_(std::move(expression_mapping))
    , variable_mapping_(std::move(variable_mapping))
{}

compiler_result::compiler_result(std::vector<diagnostic_type> diagnostics) noexcept
    : diagnostics_(std::move(diagnostics))
{}

bool compiler_result::success() const noexcept {
    return statement_ != nullptr;
}

compiler_result::operator bool() const noexcept {
    return success();
}

takatori::util::sequence_view<diagnostic_type const> compiler_result::diagnostics() const noexcept {
    return diagnostics_;
}

statement::statement& compiler_result::statement() noexcept {
    return *statement_;
}

statement::statement const& compiler_result::statement() const noexcept {
    return *statement_;
}

optional_ptr<statement::statement> compiler_result::optional_statement() noexcept {
    return optional_ptr { statement_.get() };
}

optional_ptr<statement::statement const> compiler_result::optional_statement() const noexcept {
    return optional_ptr { statement_.get() };
}

unique_object_ptr<statement::statement> compiler_result::release_statement() noexcept {
    return std::move(statement_);
}

::takatori::type::data const& compiler_result::type_of(scalar::expression const& expression) const {
    auto&& resolution = expression_mapping_->find(expression);
    if (resolution) {
        return resolution.type();
    }
    throw_exception(string_builder {}
            << "expression type is not yet resolved: "
            << expression
            << string_builder::to_string);
}

::takatori::type::data const& compiler_result::type_of(descriptor::variable const& variable) const {
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

serializer::object_scanner compiler_result::object_scanner() const noexcept {
    return serializer::object_scanner {
            variable_mapping_,
            expression_mapping_,
    };
}

} // namespace yugawara
