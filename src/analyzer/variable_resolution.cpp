#include <yugawara/analyzer/variable_resolution.h>

namespace yugawara::analyzer {

variable_resolution::variable_resolution(std::shared_ptr<::takatori::type::data const> type) noexcept
    : entity_(std::in_place_index<static_cast<std::size_t>(kind_type::unknown)>, std::move(type))
{}

variable_resolution::variable_resolution(::takatori::type::data&& type, type::repository& repo) noexcept
    : variable_resolution(repo.get(type))
{}

std::ostream& operator<<(std::ostream& out, variable_resolution const& value) {
    using kind = variable_resolution::kind_type;
    switch (value.kind()) {
        case kind::unresolved:
            return out << "unresolved";
        case kind::unknown:
            return out << *value.element<kind::unknown>();
        case kind::scalar_expression:
            return out << value.element<kind::scalar_expression>();
        case kind::table_column:
            return out << value.element<kind::table_column>();
        case kind::external:
            return out << value.element<kind::external>();
        case kind::function_call:
            return out << value.element<kind::function_call>();
        case kind::aggregation:
            return out << value.element<kind::aggregation>();
    }
    std::abort();
}

namespace {

template<variable_resolution::kind_type Kind>
bool eq(variable_resolution const& a, variable_resolution const& b) {
    auto&& r1 = a.element<Kind>();
    auto&& r2 = b.element<Kind>();
    return std::addressof(r1) == std::addressof(r2);
}

template<>
bool eq<variable_resolution::kind_type::unknown>(variable_resolution const& a, variable_resolution const& b) {
    auto&& t1 = a.element<variable_resolution::kind_type::unknown>();
    auto&& t2 = b.element<variable_resolution::kind_type::unknown>();
    return *t1 == *t2;
}

} // namespace

bool operator==(variable_resolution const& a, variable_resolution const& b) {
    if (a.kind() != b.kind()) {
        return false;
    }
    using kind = variable_resolution::kind_type;
    switch (a.kind()) {
        case kind::unresolved: return eq<kind::unresolved>(a, b);
        case kind::unknown: return eq<kind::unknown>(a, b);
        case kind::scalar_expression: return eq<kind::scalar_expression>(a, b);
        case kind::table_column: return eq<kind::table_column>(a, b);
        case kind::external: return eq<kind::external>(a, b);
        case kind::function_call: return eq<kind::function_call>(a, b);
        case kind::aggregation: return eq<kind::aggregation>(a, b);
    }
    std::abort();
}

bool operator!=(variable_resolution const& a, variable_resolution const& b) {
    return !(a == b);
}

} // namespace yugawara::analyzer
