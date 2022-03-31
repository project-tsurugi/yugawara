#include <yugawara/storage/provider.h>

#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

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

void provider::bless(relation const& element) {
    if (element.owner_ == nullptr || element.owner_ == this) {
        element.owner_ = this;
        return;
    }
    using ::takatori::util::throw_exception;
    using ::takatori::util::string_builder;
    throw_exception(std::invalid_argument { string_builder {}
            << "relation '" << element.simple_name() << "' "
            << "(" << element.kind() << ") "
            << "is already bind to " << element.owner_
            << string_builder::to_string
    });
}

void provider::unbless(relation const& element) {
    if (element.owner_ == nullptr || element.owner_ == this) {
        element.owner_ = nullptr;
        return;
    }
    using ::takatori::util::throw_exception;
    using ::takatori::util::string_builder;
    throw_exception(std::invalid_argument { string_builder {}
            << "relation '" << element.simple_name() << "' "
            << "(" << element.kind() << ") "
            << "is not owned by this provider: " << element.owner_
            << string_builder::to_string
    });
}

void provider::bless(sequence const& element) {
    if (element.owner_ == nullptr || element.owner_ == this) {
        element.owner_ = this;
        return;
    }
    using ::takatori::util::throw_exception;
    using ::takatori::util::string_builder;
    throw_exception(std::invalid_argument { string_builder {}
            << "sequence '" << element.simple_name() << "' "
            << "is already bind to " << element.owner_
            << string_builder::to_string
    });
}

void provider::unbless(sequence const& element) {
    if (element.owner_ == nullptr || element.owner_ == this) {
        element.owner_ = nullptr;
        return;
    }
    using ::takatori::util::throw_exception;
    using ::takatori::util::string_builder;
    throw_exception(std::invalid_argument { string_builder {}
            << "sequence '" << element.simple_name() << "' "
            << "is not owned by this provider: " << element.owner_
            << string_builder::to_string
    });
}

} // namespace yugawara::storage
