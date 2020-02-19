#include <yugawara/storage/index.h>

#include <takatori/util/vector_print_support.h>

namespace yugawara::storage {

index::index(
        std::shared_ptr<class table const> table,
        simple_name_type simple_name,
        std::vector<key, takatori::util::object_allocator<key>> keys,
        std::vector<column_ref, takatori::util::object_allocator<column_ref>> values,
        index_feature_set features) noexcept
    : table_(std::move(table))
    , simple_name_(std::move(simple_name))
    , keys_(std::move(keys))
    , values_(std::move(values))
    , features_(features)
{}

index::index(
        std::shared_ptr<class table const> table,
        std::string_view simple_name,
        std::initializer_list<key> keys,
        std::initializer_list<column_ref> values,
        index_feature_set features)
    : index(
            std::move(table),
            decltype(simple_name_) { simple_name },
            decltype(keys_) { keys },
            decltype(values_) { values },
            features)
{}

class table const& index::table() const noexcept {
    return *table_;
}

std::shared_ptr<class table const> const& index::shared_table() const noexcept {
    return table_;
}

index& index::table(std::shared_ptr<class table const> table) noexcept {
    table_ = std::move(table);
    return *this;
}

std::string_view index::simple_name() const noexcept {
    return simple_name_;
}

index& index::simple_name(simple_name_type simple_name) noexcept {
    simple_name_ = std::move(simple_name);
    return *this;
}

std::vector<index::key, takatori::util::object_allocator<index::key>>& index::keys() noexcept {
    return keys_;
}

std::vector<index::key, takatori::util::object_allocator<index::key>> const& index::keys() const noexcept {
    return keys_;
}

std::vector<index::column_ref, takatori::util::object_allocator<index::column_ref>>& index::values() noexcept {
    return values_;
}

std::vector<index::column_ref, takatori::util::object_allocator<index::column_ref>> const& index::values() const noexcept {
    return values_;
}

index_feature_set& index::features() noexcept {
    return features_;
}

index_feature_set const& index::features() const noexcept {
    return features_;
}

std::ostream& operator<<(std::ostream& out, index const& value) {
    return out << "index" << "("
               << "table=" << value.table().simple_name() << ", "
               << "simple_name=" << value.simple_name() << ", "
               << "keys=" << takatori::util::print_support { value.keys() } << ", "
               << "values=" << takatori::util::print_support { value.values() } << ", "
               << "features=" << value.features() << ")";
}

} // namespace yugawara::storage
