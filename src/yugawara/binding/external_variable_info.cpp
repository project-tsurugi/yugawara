#include <yugawara/binding/external_variable_info.h>

#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

external_variable_info::external_variable_info(declaration_pointer declaration) noexcept :
    declaration_ { std::move(declaration) }
{}

std::size_t external_variable_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

variable_info_kind external_variable_info::kind() const noexcept {
    return variable_info_kind::external_variable;
}

yugawara::variable::declaration const& external_variable_info::declaration() const noexcept {
    return *declaration_;
}

external_variable_info& external_variable_info::declaration(declaration_pointer declaration) noexcept {
    declaration_ = std::move(declaration);
    return *this;
}

external_variable_info::declaration_pointer const& external_variable_info::shared_declaration() const noexcept {
    return declaration_;
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

bool external_variable_info::equals(::takatori::descriptor::variable::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t external_variable_info::hash() const noexcept {
    return takatori::util::hash(declaration_);
}

std::ostream& external_variable_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding
