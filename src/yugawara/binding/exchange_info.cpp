#include <yugawara/binding/exchange_info.h>

#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

exchange_info::exchange_info(::takatori::plan::exchange const& declaration) noexcept :
    declaration_ { std::addressof(declaration) }
{}

exchange_info::exchange_info(::takatori::util::clone_tag_t, exchange_info const& other) :
    exchange_info { *other.declaration_ }
{}

exchange_info::exchange_info(::takatori::util::clone_tag_t, exchange_info&& other) :
    exchange_info { *other.declaration_ }
{}

std::size_t exchange_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

relation_info_kind exchange_info::kind() const noexcept {
    return tag;
}

exchange_info* exchange_info::clone() const& {
    return new exchange_info(::takatori::util::clone_tag, *this); // NOLINT
}

exchange_info* exchange_info::clone() && {
    return new exchange_info(::takatori::util::clone_tag, std::move(*this)); // NOLINT;
}

::takatori::plan::exchange const& exchange_info::declaration() const noexcept {
    return *declaration_;
}

bool operator==(exchange_info const& a, exchange_info const& b) noexcept {
    return a.declaration_ == b.declaration_;
}

bool operator!=(exchange_info const& a, exchange_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, exchange_info const& value) {
    return out << value.declaration();
}

bool exchange_info::equals(::takatori::descriptor::relation::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t exchange_info::hash() const noexcept {
    return takatori::util::hash(descriptor_type::tag, tag, declaration_);
}

std::ostream& exchange_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding
