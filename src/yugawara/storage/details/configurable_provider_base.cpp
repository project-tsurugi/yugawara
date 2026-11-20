#include <yugawara/storage/details/configurable_provider_base.h>

#include <stdexcept>
#include <type_traits>
#include <utility>

#include <takatori/util/exception.h>

namespace yugawara::storage::details {

namespace {

template<auto Find, class Container>
[[nodiscard]] typename Container::mapped_type find_element(
        Container const& container,
        ::takatori::util::maybe_shared_ptr<provider const> const& parent,
        std::string_view id) {
    // child first
    if (auto iter = container.find(id); iter != container.end()) {
        return iter->second;
    }
    if (parent) {
        // then parent if exists
        return ((*parent).*Find)(id);
    }
    return {};
}

template<auto Each, class Container, class Consumer>
void each_element(
        Container const& container,
        ::takatori::util::maybe_shared_ptr<provider const> const& parent,
        Consumer const& consumer) {
    for (auto&& entry : container) {
        consumer(entry.first, entry.second);
    }
    if (parent) {
        ((*parent).*Each)([&](std::string_view id, auto& element) {
            // filter elements defined in this provider
            if (auto iter = container.find(id); iter == container.end()) {
                consumer(id, element);
            }
        });
    }
}

template<class E>
constexpr bool is_bless_target_v = std::is_base_of_v<relation, std::remove_const_t<E>>
                                || std::is_same_v<sequence, std::remove_const_t<E>>
                                 ;

} // namespace

configurable_provider_base::configurable_provider_base(
        ::takatori::util::maybe_shared_ptr<provider const> parent) noexcept :
    parent_ { std::move(parent) }
{}

::takatori::util::optional_ptr<provider const> configurable_provider_base::parent() const noexcept {
    return ::takatori::util::optional_ptr<provider const> { parent_.get() };
}

std::shared_ptr<relation const> configurable_provider_base::internal_find_relation(std::string_view id) const {
    return find_element<&provider::find_relation>(relations_, parent_, id);
}

void configurable_provider_base::internal_each_relation(relation_consumer_type const& consumer) const {
    each_element<&provider::each_relation>(relations_, parent_, consumer);
}

void configurable_provider_base::internal_add_relation(std::shared_ptr<relation const> element, bool overwrite) {
    internal_add<&provider::find_relation>(relations_, std::move(element), overwrite);
}

void configurable_provider_base::internal_add_table(std::shared_ptr<table const> element, bool overwrite) {
    internal_add<&provider::find_table>(relations_, std::move(element), overwrite);
}

bool configurable_provider_base::internal_remove_relation(std::string_view id) {
    return internal_remove(relations_, id);
}

std::shared_ptr<index const> configurable_provider_base::internal_find_index(std::string_view id) const {
    return find_element<&provider::find_index>(indices_, parent_, id);
}

void configurable_provider_base::internal_each_index(index_consumer_type const& consumer) const {
    each_element<&provider::each_index>(indices_, parent_, consumer);
}

std::shared_ptr<index const> configurable_provider_base::internal_find_primary_index(class table const& table) const {
    auto const* target = std::addressof(table);
    // NOTE: we assume almost primary index name equals to its table name
    if (auto it = table_indices_.find(std::make_tuple(target, table.simple_name())); it != table_indices_.end()) {
        if (it->second->features().contains(index_feature::primary)) {
            return it->second;
        }
    }
    // If there is no such the index, then we find for all indices of the table
    for (auto it = lower_bound_table_index(target); it != table_indices_.end() && std::get<0>(it->first) == target; ++it) {
        if (it->second->features().contains(index_feature::primary)) {
            return it->second;
        }
    }
    if (parent_) {
        return parent_->find_primary_index(table);
    }
    return {};
}

void configurable_provider_base::internal_each_table_index(
        class table const& table,
        index_consumer_type const& consumer) const {
    auto const* target = std::addressof(table);
    for (auto it = lower_bound_table_index(target); it != table_indices_.end() && std::get<0>(it->first) == target; ++it) {
        consumer(std::get<1>(it->first), it->second);
    }
    if (parent_) {
        parent_->each_table_index(table, [&](std::string_view id, std::shared_ptr<index const> const& element) {
            // filter elements defined in this provider
            if (auto iter = indices_.find(id); iter == indices_.end()) {
                consumer(id, element);
            }
        });
    }
}

void configurable_provider_base::internal_add_index(std::shared_ptr<index const> element, bool overwrite) {
    using ::takatori::util::throw_exception;
    auto id = element->simple_name();
    key_type key { id };

    if (!overwrite) {
        if (parent_ && parent_->find_index(id)) {
            throw_exception(std::invalid_argument(std::string("already exists in parent provider: ") += id));
        }
        if (indices_.find(id) != indices_.end()) {
            throw_exception(std::invalid_argument(std::string("already exists: ") += id));
        }
    }

    // NOTE: this does not recover the containers from std::bad_alloc
    auto const* table = element->shared_table().get();
    if (auto iter = indices_.find(id); iter != indices_.end()) {
        // overwrite
        remove_table_index(iter->first, *iter->second);
        iter->second = element;
    } else {
        indices_.emplace(key, element);
    }
    table_indices_.emplace(table_scoped_key_type { table, std::move(key) }, std::move(element));
}

bool configurable_provider_base::internal_remove_index(std::string_view id) {
    return internal_remove(indices_, id);
}

std::shared_ptr<sequence const> configurable_provider_base::internal_find_sequence(std::string_view id) const {
    return find_element<&provider::find_sequence>(sequences_, parent_, id);
}

void configurable_provider_base::internal_each_sequence(sequence_consumer_type const& consumer) const {
    each_element<&provider::each_sequence>(sequences_, parent_, consumer);
}

void configurable_provider_base::internal_add_sequence(std::shared_ptr<sequence const> element, bool overwrite) {
    internal_add<&provider::find_sequence>(sequences_, std::move(element), overwrite);
}

bool configurable_provider_base::internal_remove_sequence(std::string_view id) {
    return internal_remove(sequences_, id);
}

void configurable_provider_base::check_element_id(
        std::string_view id,
        std::string_view simple_name,
        char const* message) {
    if (id != simple_name) {
        using ::takatori::util::throw_exception;
        throw_exception(std::invalid_argument(message));
    }
}

configurable_provider_base::table_index_map_type::const_iterator
configurable_provider_base::lower_bound_table_index(class table const* table) const {
    // NOTE: the empty ID is the smallest one of the table
    return table_indices_.lower_bound(std::make_tuple(table, std::string_view {}));
}

void configurable_provider_base::remove_table_index(std::string_view key, index const& element) noexcept {
    if (auto it = table_indices_.find(std::make_tuple(element.shared_table().get(), key));
            it != table_indices_.end()) {
        table_indices_.erase(it);
    }
}

template<auto Find, class Container, class Element>
void configurable_provider_base::internal_add(
        Container& container,
        std::shared_ptr<Element const> element,
        bool overwrite) {
    using ::takatori::util::throw_exception;
    auto id = element->simple_name();
    key_type key { id };

    if (overwrite) {
        auto [iter, success] = container.insert_or_assign(std::move(key), std::move(element));
        (void) success;
        if constexpr (is_bless_target_v<Element>) { // NOLINT
            bless(*iter->second);
        }
        return;
    }
    if (parent_ && ((*parent_).*Find)(id)) {
        throw_exception(std::invalid_argument(std::string("already exists in parent provider: ") += id));
    }
    if (auto [iter, success] = container.try_emplace(std::move(key), std::move(element)); success) {
        if constexpr (is_bless_target_v<Element>) { // NOLINT
            bless(*iter->second);
        }
        return;
    }
    throw_exception(std::invalid_argument(std::string("already exists: ") += id));
}

template<class Container>
bool configurable_provider_base::internal_remove(Container& container, std::string_view id) {
    if (auto iter = container.find(id); iter != container.end()) {
        using element_type = typename Container::mapped_type::element_type;
        if constexpr (is_bless_target_v<element_type>) { // NOLINT
            unbless(*iter->second);
        }
        if constexpr (std::is_same_v<Container, index_map_type>) { // NOLINT
            remove_table_index(iter->first, *iter->second);
        }
        // NOTE: container.erase(id) does not work
        container.erase(iter);
        return true;
    }
    return false;
}

} // namespace yugawara::storage::details
