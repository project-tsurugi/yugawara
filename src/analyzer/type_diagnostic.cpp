#include <yugawara/analyzer/type_diagnostic.h>

#include <takatori/util/clonable.h>

namespace yugawara::analyzer {

type_diagnostic::type_diagnostic(
        code_type code,
        ::takatori::document::region region,
        std::shared_ptr<::takatori::type::data const> actual_type,
        type::category_set expected_categories) noexcept
    : code_(code)
    , region_(region)
    , actual_type_(std::move(actual_type))
    , expected_categories_(expected_categories)
{}

type_diagnostic::type_diagnostic(
        type_diagnostic::code_type code,
        ::takatori::document::region region,
        ::takatori::type::data&& actual_type,
        type::category_set expected_categories)
    : type_diagnostic(
            code,
            region,
            ::takatori::util::clone_shared(std::move(actual_type)),
            expected_categories)
{}

type_diagnostic::code_type type_diagnostic::code() const noexcept {
    return code_;
}

::takatori::document::region const& type_diagnostic::region() const noexcept {
    return region_;
}

::takatori::type::data const& type_diagnostic::actual_type() const noexcept {
    return *actual_type_;
}

std::shared_ptr<::takatori::type::data const> type_diagnostic::shared_actual_type() const noexcept {
    return actual_type_;
}

type::category_set const& type_diagnostic::expected_categories() const noexcept {
    return expected_categories_;
}

std::ostream& operator<<(std::ostream& out, type_diagnostic const& value) {
    constexpr type_diagnostic_code_set type_available {
            type_diagnostic_code::unsupported_type,
            type_diagnostic_code::ambiguous_type,
            type_diagnostic_code::inconsistent_type,
    };
    if (type_available.contains(value.code())) {
        return out << "type_diagnostic("
                   << "code=" << value.code() << ", "
                   << "region=" << value.region() << ", "
                   << "actual_type=" << value.actual_type() << ", "
                   << "expected_categories=" << value.expected_categories() << ")";
    }
    return out << "type_diagnostic("
               << "code=" << value.code() << ", "
               << "region=" << value.region() << ")";
}

} // namespace yugawara::analyzer
