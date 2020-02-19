#include <yugawara/binding/table_column_info.h>

#include <takatori/util/downcast.h>
#include <takatori/util/hash.h>

namespace yugawara::binding {

table_column_info::table_column_info(yugawara::storage::column const& column) noexcept
    : column_(std::addressof(column))
{}

variable_info_kind table_column_info::kind() const noexcept {
    return variable_info_kind::table_column;
}

yugawara::storage::column const& table_column_info::column() const noexcept {
    return *column_;
}

bool operator==(table_column_info const& a, table_column_info const& b) noexcept {
    return a.column_ == b.column_;
}

bool operator!=(table_column_info const& a, table_column_info const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, table_column_info const& value) {
    return out << value.column();
}

bool table_column_info::equals(variable_info const& other) const noexcept {
    return other.kind() == tag && *this == takatori::util::unsafe_downcast<table_column_info>(other);
}

std::size_t table_column_info::hash() const noexcept {
    return takatori::util::hash(column_);
}

std::ostream& table_column_info::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::binding
