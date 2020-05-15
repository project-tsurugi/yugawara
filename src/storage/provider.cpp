#include <yugawara/storage/provider.h>

namespace yugawara::storage {

std::shared_ptr<class table const> provider::find_table(std::string_view id) const {
    return std::dynamic_pointer_cast<class table const>(find_relation(id));
}

void provider::each_table_index(
        class table const& table,
        index_consumer_type const& consumer) const {
    each_index([&](std::string_view id, std::shared_ptr<index const> const& idx) {
        if (std::addressof(idx->table()) == std::addressof(table)) {
            consumer(id, idx);
        }
    });
}

std::shared_ptr<index const> provider::find_primary_index(class table const& table) const {
    std::shared_ptr<index const> result;
    each_index([&](std::string_view, std::shared_ptr<index const> const& idx) {
        if (!result
                && std::addressof(idx->table()) == std::addressof(table)
                && idx->features().contains(index_feature::primary)) {
            result = idx;
        }
    });
    return result;
}

} // namespace yugawara::storage
