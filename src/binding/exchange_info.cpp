#include <yugawara/binding/exchange_info.h>

#include <takatori/util/downcast.h>
#include <takatori/util/hash.h>

namespace yugawara::binding {

exchange_info::exchange_info(::takatori::plan::exchange const& declaration) noexcept
    : declaration_(std::addressof(declaration))
{}

exchange_info::exchange_info(exchange_info const& other, ::takatori::util::object_creator)
    : exchange_info(*other.declaration_)
{}

exchange_info::exchange_info(exchange_info&& other, ::takatori::util::object_creator)
    : exchange_info(*other.declaration_)
{}

relation_info_kind exchange_info::kind() const noexcept {
    return tag;
}

exchange_info* exchange_info::clone(takatori::util::object_creator creator) const& {
    return creator.create_object<exchange_info>(*this, creator);
}

exchange_info* exchange_info::clone(takatori::util::object_creator creator) && {
    return creator.create_object<exchange_info>(std::move(*this), creator);
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

bool exchange_info::equals(relation_info const& other) const noexcept {
    return other.kind() == tag && *this == takatori::util::unsafe_downcast<exchange_info>(other);
}

std::size_t exchange_info::hash() const noexcept {
    return takatori::util::hash(
            takatori::descriptor::descriptor_kind::relation,
            relation_info_kind::exchange,
            declaration_);
}

std::ostream& exchange_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding
