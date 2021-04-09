#include <yugawara/binding/index_info.h>

#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

index_info::index_info(storage::index const& declaration) noexcept :
    declaration_ { std::addressof(declaration) }
{}

index_info::index_info(index_info const& other, ::takatori::util::object_creator) :
    index_info { *other.declaration_ }
{}

index_info::index_info(index_info&& other, ::takatori::util::object_creator) :
    index_info { *other.declaration_ }
{}

std::size_t index_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

relation_info_kind index_info::kind() const noexcept {
    return tag;
}

index_info* index_info::clone(takatori::util::object_creator creator) const& {
    return creator.create_object<index_info>(*this, creator);
}

index_info* index_info::clone(takatori::util::object_creator creator) && {
    return creator.create_object<index_info>(std::move(*this), creator);
}

storage::index const& index_info::declaration() const noexcept {
    return *declaration_;
}

bool operator==(index_info const& a, index_info const& b) noexcept {
    return *a.declaration_ == *b.declaration_;
}

bool operator!=(index_info const& a, index_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, index_info const& value) {
    return out << value.declaration();
}

bool index_info::equals(::takatori::descriptor::relation::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t index_info::hash() const noexcept {
    return takatori::util::hash(descriptor_type::tag, tag, declaration_);
}

std::ostream& index_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding
