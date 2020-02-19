#include <yugawara/binding/external_variable_info.h>

#include <takatori/util/downcast.h>
#include <takatori/util/hash.h>

namespace yugawara::binding {

external_variable_info::external_variable_info(variable::declaration const& declaration) noexcept
    : declaration_(std::addressof(declaration))
{}

variable_info_kind external_variable_info::kind() const noexcept {
    return variable_info_kind::external_variable;
}

yugawara::variable::declaration const& external_variable_info::declaration() const noexcept {
    return *declaration_;
}

bool operator==(external_variable_info const& a, external_variable_info const& b) noexcept {
    return a.declaration_ == b.declaration_;
}

bool operator!=(external_variable_info const& a, external_variable_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, external_variable_info const& value) {
    return out << value.declaration();
}

bool external_variable_info::equals(variable_info const& other) const noexcept {
    return other.kind() == tag && *this == takatori::util::unsafe_downcast<external_variable_info>(other);
}

std::size_t external_variable_info::hash() const noexcept {
    return takatori::util::hash(declaration_);
}

std::ostream& external_variable_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding
