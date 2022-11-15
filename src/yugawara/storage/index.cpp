#include <yugawara/storage/index.h>

#include <takatori/util/vector_print_support.h>
#include <takatori/util/optional_print_support.h>

namespace yugawara::storage {

index::index(
        std::optional<definition_id_type> definition_id,
        std::shared_ptr<class table const> table,
        simple_name_type simple_name,
        std::vector<key> keys,
        std::vector<column_ref> values,
        feature_set_type features) noexcept :
    definition_id_ { definition_id },
    table_ { std::move(table) },
    simple_name_ { std::move(simple_name) },
    keys_ { std::move(keys) },
    values_ { std::move(values) },
    features_ { features }
{}

index::index(
        std::shared_ptr<class table const> table,
        simple_name_type simple_name,
        std::vector<key> keys,
        std::vector<column_ref> values,
        feature_set_type features) noexcept :
    index {
            std::nullopt,
            std::move(table),
            std::move(simple_name),
            std::move(keys),
            std::move(values),
            features,
    }
{}

index::index(
        std::shared_ptr<class table const> table,
        std::string_view simple_name,
        std::initializer_list<key> keys,
        std::initializer_list<column_ref> values,
        feature_set_type features) :
    index {
            std::nullopt,
            std::move(table),
            decltype(simple_name_) { simple_name },
            decltype(keys_) { keys },
            decltype(values_) { values },
            features,
    }
{}

index::index(
        std::optional<definition_id_type> definition_id,
        std::shared_ptr<class table const> table,
        std::string_view simple_name,
        std::initializer_list<key> keys,
        std::initializer_list<column_ref> values,
        feature_set_type features) :
    index {
            definition_id,
            std::move(table),
            decltype(simple_name_) { simple_name },
            decltype(keys_) { keys },
            decltype(values_) { values },
            features,
    }
{}

std::optional<index::definition_id_type> index::definition_id() const noexcept {
    return definition_id_;
}

index& index::definition_id(std::optional<definition_id_type> definition_id) noexcept {
    definition_id_ = definition_id;
    return *this;
}

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

std::vector<index::key>& index::keys() noexcept {
    return keys_;
}

std::vector<index::key> const& index::keys() const noexcept {
    return keys_;
}

std::vector<index::column_ref>& index::values() noexcept {
    return values_;
}

std::vector<index::column_ref> const& index::values() const noexcept {
    return values_;
}

index::feature_set_type& index::features() noexcept {
    return features_;
}

index::feature_set_type const& index::features() const noexcept {
    return features_;
}

std::ostream& operator<<(std::ostream& out, index const& value) {
    using ::takatori::util::print_support;
    return out << "index" << "("
               << "definition_id=" << print_support { value.definition_id() } << ", "
               << "table=" << value.table().simple_name() << ", "
               << "simple_name=" << value.simple_name() << ", "
               << "keys=" << print_support { value.keys() } << ", "
               << "values=" << print_support { value.values() } << ", "
               << "features=" << value.features() << ")";
}

} // namespace yugawara::storage
