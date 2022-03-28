#include <yugawara/binding/table_info.h>

#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

table_info::table_info(declaration_pointer declaration) noexcept :
    declaration_ { std::move(declaration) }
{}

std::size_t table_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

storage::table const& table_info::declaration() const noexcept {
    return *declaration_;
}

table_info& table_info::declaration(declaration_pointer declaration) noexcept {
    declaration_ = std::move(declaration);
    return *this;
}

table_info::declaration_pointer const& table_info::shared_declaration() const noexcept {
    return declaration_;
}

bool operator==(table_info const& a, table_info const& b) noexcept {
    return a.declaration().definition_id() == b.declaration().definition_id();
}

bool operator!=(table_info const& a, table_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, table_info const& value) {
    return out << value.declaration();
}

bool table_info::equals(descriptor_type::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t table_info::hash() const noexcept {
    return takatori::util::hash(declaration_->definition_id(), descriptor_type::tag);
}

std::ostream& table_info::print_to(std::ostream& out) const {
    return out << *this;
}

table_info::descriptor_type wrap(std::shared_ptr<table_info> info) noexcept {
    return table_info::descriptor_type { std::move(info) };
}

table_info& unwrap(table_info::descriptor_type const& descriptor) {
    return ::takatori::util::unsafe_downcast<table_info>(descriptor.entity());
}

} // namespace yugawara::binding
