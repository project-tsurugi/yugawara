#include <yugawara/binding/function_info.h>

#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

function_info::function_info(declaration_pointer declaration) noexcept :
    declaration_ { std::move(declaration) }
{}

std::size_t function_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

function::declaration const& function_info::declaration() const noexcept {
    return *declaration_;
}

function_info& function_info::declaration(declaration_pointer declaration) noexcept {
    declaration_ = std::move(declaration);
    return *this;
}

function_info::declaration_pointer const& function_info::shared_declaration() const noexcept {
    return declaration_;
}

bool operator==(function_info const& a, function_info const& b) noexcept {
    return a.declaration().definition_id() == b.declaration().definition_id();
}

bool operator!=(function_info const& a, function_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, function_info const& value) {
    return out << value.declaration();
}

bool function_info::equals(::takatori::descriptor::function::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t function_info::hash() const noexcept {
    return takatori::util::hash(declaration_->definition_id(), descriptor_type::tag);
}

std::ostream& function_info::print_to(std::ostream& out) const {
    return out << *this;
}

function_info::descriptor_type wrap(std::shared_ptr<function_info> info) noexcept {
    return function_info::descriptor_type { std::move(info) };
}

function_info& unwrap(function_info::descriptor_type const& descriptor) {
    return ::takatori::util::unsafe_downcast<function_info>(descriptor.entity());
}

} // namespace yugawara::binding
