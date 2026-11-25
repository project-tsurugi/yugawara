#include <yugawara/storage/partition_key_element.h>

namespace yugawara::storage {

partition_key_element::partition_key_element(
        class column const& column,
        name_type column_name,
        prefix_extractor_type prefix_extractor) noexcept :
    column_ { std::addressof(column) },
    column_name_ { std::move(column_name) },
    prefix_extractor_ { prefix_extractor }
{}

partition_key_element::partition_key_element(class column const& column) :
    partition_key_element {
            column,
            name_type { column.simple_name() },
    }
{}

column const& partition_key_element::column() const noexcept {
    return *column_;
}

std::string_view partition_key_element::column_name() const noexcept {
    return column_name_;
}

partition_key_element::prefix_extractor_type const& partition_key_element::prefix_extractor() const noexcept {
    return prefix_extractor_;
}

bool operator==(partition_key_element const& a, partition_key_element const& b) noexcept {
    return a.column() == b.column()
        && a.column_name() == b.column_name()
        && a.prefix_extractor() == b.prefix_extractor();
}

bool operator!=(partition_key_element const& a, partition_key_element const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, partition_key_element const& value) {
    return out << "partition_key_element("
               << "column=" << value.column().simple_name() << ", "
               << "column_name=" << value.column_name() << ", "
               << "prefix_extractor=" << value.prefix_extractor() << ")";
}

} // namespace yugawara::storage
