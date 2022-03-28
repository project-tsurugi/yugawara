#include <yugawara/binding/schema_info.h>

#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

schema_info::schema_info(declaration_pointer declaration) noexcept :
    declaration_ { std::move(declaration) }
{}

std::size_t schema_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

schema::declaration const& schema_info::declaration() const noexcept {
    return *declaration_;
}

schema_info& schema_info::declaration(declaration_pointer declaration) noexcept {
    declaration_ = std::move(declaration);
    return *this;
}

schema_info::declaration_pointer const& schema_info::shared_declaration() const noexcept {
    return declaration_;
}

bool operator==(schema_info const& a, schema_info const& b) noexcept {
    return a.declaration().definition_id() == b.declaration().definition_id();
}

bool operator!=(schema_info const& a, schema_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, schema_info const& value) {
    return out << value.declaration();
}

bool schema_info::equals(descriptor_type::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t schema_info::hash() const noexcept {
    return takatori::util::hash(declaration_->definition_id(), descriptor_type::tag);
}

std::ostream& schema_info::print_to(std::ostream& out) const {
    return out << *this;
}

schema_info::descriptor_type wrap(std::shared_ptr<schema_info> info) noexcept {
    return schema_info::descriptor_type { std::move(info) };
}

schema_info& unwrap(schema_info::descriptor_type const& descriptor) {
    return ::takatori::util::unsafe_downcast<schema_info>(descriptor.entity());
}

} // namespace yugawara::binding
