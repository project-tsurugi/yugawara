#pragma once

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>

#include <takatori/util/clonable.h>
#include <takatori/util/exception.h>
#include <takatori/util/maybe_shared_ptr.h>
#include <takatori/util/optional_ptr.h>

#include <yugawara/util/maybe_shared_lock.h>

#include "provider.h"

namespace yugawara::storage {

/**
 * @brief an implementation of provider that can configurable its contents.
 * @tparam Mutex the mutex type, must satisfy *DefaultConstructible* and *BasicLockable*.
 * @note This class works as thread-safe only if the mutex works right
 */
template<class Mutex>
class basic_configurable_provider : public provider {
public:
    /// @brief the mutex type.
    using mutex_type = Mutex;

    /// @brief the writer lock type.
    using writer_lock_type = std::unique_lock<mutex_type>;

    /// @brief the readers lock type.
    using reader_lock_type = util::maybe_shared_lock<mutex_type>;

    /**
     * @brief creates a new object.
     * @param parent the parent provider (nullable)
     */
    explicit basic_configurable_provider(
            ::takatori::util::maybe_shared_ptr<provider const> parent = {}) noexcept :
        parent_ { std::move(parent) }
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
     * @param element the relation to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<relation> const& add_relation(std::shared_ptr<relation> element, bool overwrite = false) {
        return internal_add<&provider::find_relation>(relations_, std::move(element), overwrite);
    }

    /// @copydoc add_relation(std::shared_ptr<relation>, bool)
    std::shared_ptr<relation> const& add_relation(relation&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        return add_relation(std::move(shared), overwrite);
    }

    /**
     * @brief adds a table.
     * @param element the table to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<table> add_table(std::shared_ptr<table> element, bool overwrite = false) {
        internal_add<&provider::find_table>(relations_, element, overwrite);
        return element;
    }

    /// @copydoc add_table(std::shared_ptr<table>, bool)
    std::shared_ptr<table> add_table(table&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        return add_table(std::move(shared), overwrite);
    }

    /**
     * @brief adds a relation.
     * @attention deprecated: use add_relation(std::shared_ptr<relation>, bool) instead
     * @param id the relation ID, must be the simple name of the relation
     * @param element the relation to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    [[deprecated]]
    std::shared_ptr<relation> const& add_relation(std::string_view id, std::shared_ptr<relation> element, bool overwrite = false) {
        if (id != element->simple_name()) {
            using ::takatori::util::throw_exception;
            throw_exception(std::invalid_argument("inconsistent relation ID"));
        }
        return add_relation(std::move(element), overwrite);
    }

    /// @copydoc add_relation(std::string_view id, std::shared_ptr<relation>, bool)
    [[deprecated]]
    std::shared_ptr<relation> const& add_relation(std::string_view id, relation&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        return add_relation(id, std::move(shared), overwrite);
    }

    /**
     * @brief adds a table.
     * @attention deprecated: use add_table(std::shared_ptr<table>, bool) instead
     * @param id the table ID, must be the simple name of the table
     * @param element the table to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    [[deprecated]]
    std::shared_ptr<table> add_table(std::string_view id, std::shared_ptr<table> element, bool overwrite = false) {
        if (id != element->simple_name()) {
            using ::takatori::util::throw_exception;
            throw_exception(std::invalid_argument("inconsistent table ID"));
        }
        return add_table(std::move(element), overwrite);
    }

    /// @copydoc add_table(std::string_view id, std::shared_ptr<table>, bool)
    [[deprecated]]
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

    std::shared_ptr<index const> find_primary_index(class table const& table) const override {
        reader_lock_type lock { mutex_ };

        // NOTE: we assume almost primary index name equals to its table name
        if (auto it = indices_.find(table.simple_name()); it != indices_.end()) {
            auto&& idx = *it->second;
            if (std::addressof(idx.table()) == std::addressof(table)
                    && idx.features().contains(index_feature::primary)) {
                return it->second;
            }
        }
        // If there is no such the index, then we find for all indices
        for (auto&& entry : indices_) {
            auto&& idx = *entry.second;
            if (std::addressof(idx.table()) == std::addressof(table)
                    && idx.features().contains(index_feature::primary)) {
                return entry.second;
            }
        }
        if (parent_) {
            return find_primary_index(table);
        }
        return {};
    }

    /**
     * @brief adds an index.
     * @param element the index to add
     * @param overwrite true to overwrite the existing entry
     * @return this
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<index> const& add_index(std::shared_ptr<index> element, bool overwrite = false) {
        return internal_add<&provider::find_index>(indices_, std::move(element), overwrite);
    }

    /// @copydoc add_index(std::shared_ptr<index>, bool)
    std::shared_ptr<index> const& add_index(index&& element, bool overwrite = false) {
        auto shared = std::make_shared<index>(std::move(element));
        return add_index(std::move(shared), overwrite);
    }

    /**
     * @brief adds an index.
     * @attention deprecated: use add_index(std::shared_ptr<index>, bool) instead
     * @param id the index ID
     * @param element the index to add
     * @param overwrite true to overwrite the existing entry
     * @return this
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    [[deprecated]]
    std::shared_ptr<index> const& add_index(std::string_view id, std::shared_ptr<index> element, bool overwrite = false) {
        if (id != element->simple_name()) {
            using ::takatori::util::throw_exception;
            throw_exception(std::invalid_argument("inconsistent index ID"));
        }
        return add_index(indices_, std::move(element), overwrite);
    }

    /// @copydoc add_index(std::string_view id, std::shared_ptr<index>, bool)
    [[deprecated]]
    std::shared_ptr<index> const& add_index(std::string_view id, index&& element, bool overwrite = false) {
        auto shared = std::make_shared<index>(std::move(element));
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

    [[nodiscard]] std::shared_ptr<class sequence const> find_sequence(std::string_view id) const override {
        return internal_find<&provider::find_sequence>(sequences_, id);
    }

    /**
     * @brief provides all sequences in this provider or its parents.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     * @note The hidden entries in parent provider will not occur in the consumer.
     */
    void each_sequence(sequence_consumer_type const& consumer) const override {
        internal_each<&provider::each_sequence>(sequences_, consumer);
    }

    /**
     * @brief adds a sequence.
     * @param element the sequence to add
     * @param overwrite true to overwrite the existing entry
     * @return this
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in parent providers if `overwrite=true`
     */
    std::shared_ptr<sequence> const& add_sequence(std::shared_ptr<sequence> element, bool overwrite = false) {
        return internal_add<&provider::find_sequence>(sequences_, std::move(element), overwrite);
    }

    /// @copydoc add_sequence(std::shared_ptr<sequence>, bool)
    std::shared_ptr<sequence> const& add_sequence(sequence&& element, bool overwrite = false) {
        auto shared = std::make_shared<sequence>(std::move(element));
        return add_sequence(std::move(shared), overwrite);
    }

    /**
     * @brief removes the sequence in this provider.
     * @param id the sequence ID
     * @return true if successfully removed
     * @return false if there is no such a sequence
     */
    bool remove_sequence(std::string_view id) {
        return internal_remove(sequences_, id);
    }

    /**
     * @brief returns the parent provider.
     * @return the parent provider
     * @return empty if there are no parents
     */
    [[nodiscard]] ::takatori::util::optional_ptr<provider const> parent() const noexcept {
        return takatori::util::optional_ptr<provider const> { parent_.get() };
    }

private:
    using key_type = std::string;
    using relation_value_type = std::shared_ptr<relation>;
    using relation_map_type = std::map<
            key_type,
            relation_value_type,
            std::less<>>;

    using index_value_type = std::shared_ptr<index>;
    using index_map_type = std::map<
            key_type,
            index_value_type,
            std::less<>>;

    using sequence_value_type = std::shared_ptr<sequence>;
    using sequence_map_type = std::map<
            key_type,
            sequence_value_type,
            std::less<>>;

    ::takatori::util::maybe_shared_ptr<provider const> parent_;
    relation_map_type relations_;
    index_map_type indices_;
    sequence_map_type sequences_;
    mutable mutex_type mutex_ {};

    template<class Container>
    using element_type = typename Container::value_type::second_type::element_type;

    template<auto Find, class Container>
    [[nodiscard]] std::shared_ptr<element_type<Container> const> internal_find(
            Container& container,
            std::string_view id) const {
        reader_lock_type lock { mutex_ };
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
        reader_lock_type lock { mutex_ };
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

    template<class E>
    static constexpr bool is_bless_target_v = std::is_base_of_v<relation, E> || std::is_same_v<sequence, E>;

    template<auto Find, class Container>
    std::shared_ptr<element_type<Container>> const& internal_add(
            Container& container,
            std::shared_ptr<element_type<Container>> element,
            bool overwrite) {
        using element_type = element_type<Container>;

        using ::takatori::util::throw_exception;
        auto id = element->simple_name();
        key_type key { id };

        writer_lock_type lock { mutex_ };
        if (overwrite) {
            auto [iter, success] = container.insert_or_assign(std::move(key), std::move(element));
            (void) success;
            if constexpr (is_bless_target_v<element_type>) { // NOLINT
                bless(*iter->second);
            }
            return iter->second;
        }
        if (parent_ && ((*parent_).*Find)(id)) {
            throw_exception(std::invalid_argument(std::string("already exists in parent provider: ") += id));
        }
        if (auto [iter, success] = container.try_emplace(std::move(key), std::move(element)); success) {
            if constexpr (is_bless_target_v<element_type>) { // NOLINT
                bless(*iter->second);
            }
            return iter->second;
        }
        throw_exception(std::invalid_argument(std::string("already exists: ") += id));
    }

    template<class Container>
    bool internal_remove(
            Container& container,
            std::string_view id) {
        writer_lock_type lock { mutex_ };
        if (auto iter = container.find(id); iter != container.end()) {
            using element_type = element_type<Container>;
            if constexpr (is_bless_target_v<element_type>) { // NOLINT
                unbless(*iter->second);
            }
            // NOTE: container.erase(id) does not work
            container.erase(iter);
            return true;
        }
        return false;
    }
};

} // namespace yugawara::storage
