#include <yugawara/storage/basic_prototype_processor.h>

#include <tsl/hopscotch_map.h>

#include <takatori/util/clonable.h>
#include <takatori/util/assertion.h>
#include <takatori/util/fail.h>
#include <takatori/util/string_builder.h>

namespace yugawara::storage {

using ::takatori::util::clone_shared;
using ::takatori::util::fail;
using ::takatori::util::string_builder;

static std::shared_ptr<index> create_index_copy_same_origin(index const& prototype) {
    return std::make_shared<index>(prototype);
}

static std::shared_ptr<index> create_index_copy(
        index const& prototype,
        std::shared_ptr<table const> target) {
    if (std::addressof(prototype.table()) == target.get()) {
        return create_index_copy_same_origin(prototype);
    }

    // prepare column name map
    ::tsl::hopscotch_map<std::string_view, std::reference_wrapper<column const>> column_name_map;
    column_name_map.reserve(target->columns().size());
    for (auto const& column : target->columns()) {
        column_name_map.emplace(column.simple_name(), std::cref(column));
    }

    // rebind keys
    std::vector<index::key> new_keys {};
    new_keys.reserve(prototype.keys().size());
    for (auto const& old : prototype.keys()) {
        BOOST_ASSERT(std::addressof(old.column().owner()) == std::addressof(prototype.table())); // NOLINT
        if (auto it = column_name_map.find(old.column().simple_name()); it != column_name_map.end()) {
            new_keys.emplace_back(it->second, old.direction());
        } else {
            fail(string_builder {}
                    << "missing index column " << prototype
                    << "." << old.column().simple_name() << " in table " << *target
                    << string_builder::to_string);
        }
    }

    // rebind values
    std::vector<index::column_ref> new_values {};
    new_values.reserve(prototype.values().size());
    for (auto const& old : prototype.values()) {
        if (auto it = column_name_map.find(old.get().simple_name()); it != column_name_map.end()) {
            new_values.emplace_back(it->second);
        } else {
            fail(string_builder {}
                    << "missing index column " << prototype
                    << "." << old.get().simple_name() << " in table " << *target
                    << string_builder::to_string);
        }
    }

    return std::make_shared<index>(
            prototype.definition_id(),
            std::move(target),
            index::simple_name_type { prototype.simple_name() },
            std::move(new_keys),
            std::move(new_values),
            prototype.features());
}

std::shared_ptr<index const> basic_prototype_processor::operator()(
        schema::declaration const& location,
        index const& prototype,
        diagnostic_consumer_type const& diagnostic_consumer) {
    auto copy_table = clone_shared(prototype.table());
    auto& table_ref = *copy_table;
    auto copy_index = create_index_copy(prototype, std::move(copy_table));

    if (!ensure(location, table_ref, *copy_index, diagnostic_consumer)) {
        return {};
    }
    return copy_index;
}

std::shared_ptr<index const> basic_prototype_processor::operator()(
        schema::declaration const& location,
        index const& prototype,
        std::shared_ptr<table const> rebind_target,
        diagnostic_consumer_type const& diagnostic_consumer) {
    auto copy_index = create_index_copy(prototype, std::move(rebind_target));
    if (!ensure(location, *copy_index, diagnostic_consumer)) {
        return {};
    }
    return copy_index;
}

bool basic_prototype_processor::ensure(
        schema::declaration const&,
        table&, index&,
        diagnostic_consumer_type const&) {
    return true;
}

bool basic_prototype_processor::ensure(
        schema::declaration const&,
        index&,
        diagnostic_consumer_type const&) {
    return true;
}
} // namespace yugawara::storage
