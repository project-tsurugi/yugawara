#include <yugawara/binding/table_column_info.h>

#include <takatori/util/hash.h>

#include "class_id_util.h"

namespace yugawara::binding {

table_column_info::table_column_info(yugawara::storage::column const& column) noexcept
    : column_(std::addressof(column))
{}

std::size_t table_column_info::class_id() const noexcept {
    return class_id_delegate(*this);
}

variable_info_kind table_column_info::kind() const noexcept {
    return tag;
}

yugawara::storage::column const& table_column_info::column() const noexcept {
    return *column_;
}

bool operator==(table_column_info const& a, table_column_info const& b) noexcept {
    return *a.column_ == *b.column_;
}

bool operator!=(table_column_info const& a, table_column_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, table_column_info const& value) {
    return out << value.column();
}

bool table_column_info::equals(::takatori::descriptor::variable::entity_type const& other) const noexcept {
    return equals_delegate(*this, other);
}

std::size_t table_column_info::hash() const noexcept {
    return takatori::util::hash(column_);
}

std::ostream& table_column_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding
