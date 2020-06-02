#include <yugawara/compiler_result.h>

#include <yugawara/binding/extract.h>

namespace yugawara {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace statement = ::takatori::statement;

using diagnostic_type = compiler_result::diagnostic_type;
using info_type = compiler_result::info_type;

using ::takatori::util::optional_ptr;
using ::takatori::util::sequence_view;
using ::takatori::util::unique_object_ptr;

yugawara::compiler_result::compiler_result(
        unique_object_ptr<statement::statement> statement,
        info_type info) noexcept :
    statement_ { std::move(statement) },
    info_ { std::move(info) }
{}

compiler_result::compiler_result(std::vector<diagnostic_type> diagnostics) noexcept :
    diagnostics_ { std::move(diagnostics) }
{}

bool compiler_result::success() const noexcept {
    return statement_ != nullptr;
}

compiler_result::operator bool() const noexcept {
    return success();
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

info_type& compiler_result::info() noexcept {
    return info_;
}

info_type const& compiler_result::info() const noexcept {
    return info_;
}

sequence_view<diagnostic_type const> compiler_result::diagnostics() const noexcept {
    return diagnostics_;
}

::takatori::type::data const& compiler_result::type_of(scalar::expression const& expression) const {
    return info_.type_of(expression);
}

::takatori::type::data const& compiler_result::type_of(descriptor::variable const& variable) const {
    return info_.type_of(variable);
}

serializer::object_scanner compiler_result::object_scanner() const noexcept {
    return info_.object_scanner();
}

} // namespace yugawara
