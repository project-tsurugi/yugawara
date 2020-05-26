#include <yugawara/storage/table.h>

#include <takatori/tree/tree_element_vector_forward.h>

namespace yugawara::storage {

table::table(table::simple_name_type simple_name, takatori::util::reference_vector<column> columns) noexcept
    : simple_name_(std::move(simple_name))
    , columns_(*this, std::move(columns))
{}

table::table(
        std::string_view simple_name,
        std::initializer_list<column> columns)
    : table(
            decltype(simple_name_) { simple_name },
            { columns.begin(), columns.end() })
{}

table::table(table const& other, takatori::util::object_creator creator)
    : table(
            decltype(simple_name_) { other.simple_name_, creator.allocator() },
            takatori::tree::forward(creator, other.columns_))
{}

table::table(table&& other, takatori::util::object_creator creator)
    : table(
            decltype(simple_name_) { std::move(other.simple_name_), creator.allocator() },
            takatori::tree::forward(creator, std::move(other.columns_)))
{}

table* table::clone(takatori::util::object_creator creator) const& {
    return creator.create_object<table>(*this, creator);
}

table* table::clone(takatori::util::object_creator creator) && {
    return creator.create_object<table>(std::move(*this), creator);
}

relation_kind table::kind() const noexcept {
    return tag;
}

std::string_view table::simple_name() const noexcept {
    return simple_name_;
}

table& table::simple_name(table::simple_name_type simple_name) noexcept {
    simple_name_ = std::move(simple_name);
    return *this;
}

column_list_view table::columns() const noexcept {
    return column_list_view { columns_.begin(), columns_.end() };
}

table::column_vector_type& table::columns() noexcept {
    return columns_;
}

std::ostream& operator<<(std::ostream& out, table const& value) {
    return out << value.kind() << "("
               << "simple_name=" << value.simple_name() << ", "
               << "columns=" << value.columns_ << ")";
}

std::ostream& table::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::storage
