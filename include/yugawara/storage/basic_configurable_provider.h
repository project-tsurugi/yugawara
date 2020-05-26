#pragma once

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

#include <takatori/util/clonable.h>
#include <takatori/util/exception.h>
#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>

#include "provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of provider that can configurable its contents.
 * @tparam Mutex the mutex type, must satisfy *DefaultConstructible* and *BasicLockable*
 * @note This class works as thread-safe only if the mutex works right
 */
template<class Mutex>
class basic_configurable_provider : public provider {
public:
    /// @brief the mutex type.
    using mutex_type = Mutex;

    /**
     * @brief creates a new object.
     * @param parent the parent provider (nullable)
     * @param creator the object creator
     */
    explicit basic_configurable_provider(
            ::takatori::util::maybe_shared_ptr<provider const> parent = {},
            ::takatori::util::object_creator creator = {}) noexcept
        : parent_(std::move(parent))
        , relations_(creator.allocator(std::in_place_type<typename decltype(relations_)::value_type>))
        , indices_(creator.allocator(std::in_place_type<typename decltype(indices_)::value_type>))
    {}

    std::shared_ptr<relation const> find_relation(std::string_view id) const override {
        return internal_find<&provider::find_relation>(relations_, id);
    }

    /**
     * @brief provides all relations in this provider or its parents.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     * @note The hidden entries in parent provider will not occur in the consumer.
     */
    void each_relation(relation_consumer_type consumer) const override {
        internal_each<&provider::each_relation>(relations_, consumer);
    }

    /**
     * @brief adds a relation.
     * @param id the relation ID
     * @param element the relation to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<relation> const& add_relation(std::string_view id, std::shared_ptr<relation> element, bool overwrite = false) {
        return internal_add<&provider::find_relation>(relations_, id, std::move(element), overwrite);
    }

    /// @copydoc add_relation()
    std::shared_ptr<relation> const& add_relation(std::string_view id, relation&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        return add_relation(id, std::move(shared), overwrite);
    }

    /**
     * @brief adds a table.
     * @param id the table ID
     * @param element the table to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<table> add_table(std::string_view id, std::shared_ptr<table> element, bool overwrite = false) {
        internal_add<&provider::find_table>(relations_, id, element, overwrite);
        return element;
    }

    /// @copydoc add_table()
    std::shared_ptr<table> add_table(std::string_view id, table&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        return add_table(id, std::move(shared), overwrite);
    }

    /**
     * @brief removes the relation in this provider.
     * @param id the relation ID
     * @return true if successfully removed
     * @return false if there is no such a relation
     */
    bool remove_relation(std::string_view id) {
        return internal_remove(relations_, id);
    }

    [[nodiscard]] std::shared_ptr<class index const> find_index(std::string_view id) const override {
        return internal_find<&provider::find_index>(indices_, id);
    }

    /**
     * @brief provides all indices in this provider or its parents.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     * @note The hidden entries in parent provider will not occur in the consumer.
     */
    void each_index(index_consumer_type const& consumer) const override {
        internal_each<&provider::each_index>(indices_, consumer);
    }

    /**
     * @brief adds an index.
     * @param id the index ID
     * @param element the index to add
     * @param overwrite true to overwrite the existing entry
     * @return this
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<index> const& add_index(std::string_view id, std::shared_ptr<index> element, bool overwrite = false) {
        return internal_add<&provider::find_index>(indices_, id, std::move(element), overwrite);
    }

    /// @copydoc add_index()
    std::shared_ptr<index> const& add_index(std::string_view id, index&& element, bool overwrite = false) {
        auto shared = get_object_creator().template create_shared<index>(std::move(element));
        return add_index(id, std::move(shared), overwrite);
    }

    /**
     * @brief removes the index in this provider.
     * @param id the index ID
     * @return true if successfully removed
     * @return false if there is no such an index
     */
    bool remove_index(std::string_view id) {
        return internal_remove(indices_, id);
    }

    /**
     * @brief returns the parent provider.
     * @return the parent provider
     * @return empty if there are no parents
     */
    [[nodiscard]] ::takatori::util::optional_ptr<provider const> parent() const noexcept {
        return takatori::util::optional_ptr<provider const> { parent_.get() };
    }

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const {
        return relations_.get_allocator();
    }

private:
    using key_type = std::basic_string<char, std::char_traits<char>, takatori::util::object_allocator<char>>;
    using relation_value_type = std::shared_ptr<relation>;
    using relation_map_type = std::map<
            key_type,
            relation_value_type,
            std::less<>,
            takatori::util::object_allocator<std::pair<key_type const, relation_value_type>>>;

    using index_value_type = std::shared_ptr<index>;
    using index_map_type = std::map<
            key_type,
            index_value_type,
            std::less<>,
            takatori::util::object_allocator<std::pair<key_type const, index_value_type>>>;

    ::takatori::util::maybe_shared_ptr<provider const> parent_;
    relation_map_type relations_;
    index_map_type indices_;
    mutable mutex_type mutex_ {};

    template<class Container>
    using element_type = typename Container::value_type::second_type::element_type;

    template<auto Find, class Container>
    [[nodiscard]] std::shared_ptr<element_type<Container> const> internal_find(
            Container& container,
            std::string_view id) const {
        std::lock_guard lock { mutex_ };
        // child first
        if (auto iter = container.find(id); iter != container.end()) {
            return iter->second;
        }
        if (parent_) {
            // then parent if exists
            return ((*parent_).*Find)(id);
        }
        return {};
    }

    template<auto Each, class Container>
    void internal_each(
            Container& container,
            consumer_type<element_type<Container>> const& consumer) const {
        std::lock_guard lock { mutex_ };
        for (auto&& entry : container) {
            consumer(entry.first, entry.second);
        }
        if (parent_) {
            ((*parent_).*Each)([&](std::string_view id, auto& element) {
                // filter elements defined in this provider
                if (auto iter = container.find(id); iter == container.end()) {
                    consumer(id, element);
                }
            });
        }
    }

    template<auto Find, class Container>
    std::shared_ptr<element_type<Container>> const& internal_add(
            Container& container,
            std::string_view id,
            std::shared_ptr<element_type<Container>> element,
            bool overwrite) {
        using ::takatori::util::throw_exception;
        key_type key { id, get_object_creator().allocator(std::in_place_type<char>) };

        std::lock_guard lock { mutex_ };
        if (overwrite) {
            auto [iter, success] = container.insert_or_assign(std::move(key), std::move(element));
            (void) success;
            return iter->second;
        }
        if (parent_ && ((*parent_).*Find)(id)) {
            throw_exception(std::invalid_argument(std::string("already exists in parent provider: ") += id));
        }
        if (auto [iter, success] = container.try_emplace(std::move(key), std::move(element)); success) {
            return iter->second;
        }
        throw_exception(std::invalid_argument(std::string("already exists: ") += id));
    }

    template<class Container>
    bool internal_remove(
            Container& container,
            std::string_view id) {
        std::lock_guard lock { mutex_ };
        if (auto iter = container.find(id); iter != container.end()) {
            // NOTE: container.erase(id) does not work
            container.erase(iter);
            return true;
        }
        return false;
    }
};

} // namespace yugawara::storage
