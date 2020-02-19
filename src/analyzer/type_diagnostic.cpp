#include <yugawara/analyzer/type_diagnostic.h>

#include <takatori/util/clonable.h>

namespace yugawara::analyzer {

type_diagnostic::type_diagnostic(
        code_type code,
        ::takatori::scalar::expression const& location,
        std::shared_ptr<::takatori::type::data const> actual_type,
        type_diagnostic::category_set expected_categories) noexcept
    : code_(code)
    , location_(std::addressof(location))
    , actual_type_(std::move(actual_type))
    , expected_categories_(expected_categories)
{}

type_diagnostic::type_diagnostic(
        type_diagnostic::code_type code,
        ::takatori::scalar::expression const& location,
        ::takatori::type::data&& actual_type,
        type_diagnostic::category_set expected_categories)
    : type_diagnostic(
            code,
            location,
            ::takatori::util::clone_shared(std::move(actual_type)),
            expected_categories)
{}

type_diagnostic::code_type type_diagnostic::code() const noexcept {
    return code_;
}

::takatori::scalar::expression const& type_diagnostic::location() const noexcept {
    return *location_;
}

::takatori::type::data const& type_diagnostic::actual_type() const noexcept {
    return *actual_type_;
}

std::shared_ptr<::takatori::type::data const> type_diagnostic::shared_actual_type() const noexcept {
    return actual_type_;
}

type_diagnostic::category_set const& type_diagnostic::expected_categories() const noexcept {
    return expected_categories_;
}

std::ostream& operator<<(std::ostream& out, type_diagnostic const& value) {
    return out << "type_diagnostic("
               << "code=" << value.code() << ", "
               << "location=" << value.location() << ", "
               << "actual_type=" << value.actual_type() << ", "
               << "expected_categories=" << value.expected_categories() << ")";
}

} // namespace yugawara::analyzer
