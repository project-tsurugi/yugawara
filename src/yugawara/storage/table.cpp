#include <yugawara/storage/table.h>

#include <takatori/tree/tree_element_vector_forward.h>
#include <takatori/util/optional_print_support.h>

namespace yugawara::storage {

table::table(
        std::optional<definition_id_type> definition_id,
        simple_name_type simple_name,
        ::takatori::util::reference_vector<column> columns) noexcept :
    definition_id_ { definition_id },
    simple_name_ { std::move(simple_name) },
    columns_ { *this, std::move(columns) }
{}

table::table(
        simple_name_type simple_name,
        ::takatori::util::reference_vector<column> columns) noexcept :
    table {
            std::nullopt,
            std::move(simple_name),
            std::move(columns),
    }
{}

table::table(
        std::string_view simple_name,
        std::initializer_list<column> columns) :
    table {
            std::nullopt,
            decltype(simple_name_) { simple_name },
            { columns.begin(), columns.end() }
    }
{}

table::table(
        std::optional<definition_id_type> definition_id,
        std::string_view simple_name,
        std::initializer_list<column> columns) :
    table {
            definition_id,
            decltype(simple_name_) { simple_name },
            { columns.begin(), columns.end() }
    }
{}

table::table(::takatori::util::clone_tag_t, table const& other)
    : table {
            other.definition_id_,
            decltype(simple_name_) { other.simple_name_ },
            ::takatori::tree::forward(other.columns_),
    }
{}

table::table(::takatori::util::clone_tag_t, table&& other)
    : table {
            other.definition_id_,
            decltype(simple_name_) { std::move(other.simple_name_) },
            takatori::tree::forward(std::move(other.columns_)),
    }
{}

table* table::clone() const& {
    return new table(::takatori::util::clone_tag, *this); // NOLINT
}

table* table::clone() && {
    return new table(::takatori::util::clone_tag, std::move(*this)); // NOLINT;
}

relation_kind table::kind() const noexcept {
    return tag;
}

std::string_view table::simple_name() const noexcept {
    return simple_name_;
}

std::optional<table::definition_id_type> table::definition_id() const noexcept {
    return definition_id_;
}

table& table::definition_id(std::optional<definition_id_type> definition_id) noexcept {
    definition_id_ = definition_id;
    return *this;
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
    using ::takatori::util::print_support;
    return out << value.kind() << "("
               << "definition_id=" << print_support { value.definition_id() } << ", "
               << "simple_name=" << value.simple_name() << ", "
               << "columns=" << value.columns_ << ")";
}

std::ostream& table::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::storage
