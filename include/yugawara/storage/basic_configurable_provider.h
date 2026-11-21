#pragma once

#include <memory>
#include <mutex>
#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>

#include <takatori/util/clonable.h>
#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/util/maybe_shared_lock.h>

#include "provider.h"
#include "details/configurable_provider_base.h"

namespace yugawara::storage {

/**
 * @brief an implementation of provider that can configurable its contents.
 * @tparam Mutex the mutex type, must satisfy *BasicLockable*.
 *      The read operations, which are the member functions declared in storage::provider
 *      (find_*() and each_*()), and the write operations, which are add_*() and remove_*(),
 *      acquire exclusive locks.
 *      However, if the mutex has lock_shared() and unlock_shared(), as *SharedLockable* types do,
 *      the read operations acquire shared locks instead.
 * @note This class works as thread-safe only if the mutex works right and the parent chain is thread-safe.
 * @note The parent provider is the provider passed as `parent` to the constructor,
 *      and this provider looks up entries also in the parent provider.
 *      The parent chain consists of the parent provider, its parent provider, and so on.
 * @note Any operation may call the read operations of the parent provider while holding the lock of this provider.
 * @attention The consumers of each_*() are called while holding the lock of this provider,
 *      and may be called while holding the locks of the providers in its parent chain.
 *      The consumers must not call, directly or indirectly, the member functions of this provider
 *      or the providers in its parent chain,
 *      and must not wait for other threads that may call them directly or indirectly.
 */
template<class Mutex>
class basic_configurable_provider : public details::configurable_provider_base {
public:
    /// @brief the mutex type.
    using mutex_type = Mutex;

    /// @brief the writer lock type.
    using writer_lock_type = std::unique_lock<mutex_type>;

    /// @brief the readers lock type.
    using reader_lock_type = util::maybe_shared_lock<mutex_type>;

    /**
     * @brief creates a new object.
     * @details This is available only if the mutex is constructible from the given arguments.
     *      Without the arguments, the mutex must be default constructible.
     * @tparam MutexArgs the argument types of the mutex constructor
     * @param parent the parent provider (nullable)
     * @param args the arguments of the mutex constructor
     * @throws any exceptions thrown by the mutex constructor
     */
    template<
            class... MutexArgs,
            std::enable_if_t<std::is_constructible_v<mutex_type, MutexArgs...>, std::nullptr_t> = nullptr>
    explicit basic_configurable_provider(
            ::takatori::util::maybe_shared_ptr<provider const> parent = {},
            MutexArgs&&... args) noexcept(std::is_nothrow_constructible_v<mutex_type, MutexArgs...>) :
        configurable_provider_base { std::move(parent) },
        // NOTE: use parentheses as well as std::is_constructible, to accept the arguments with narrowing conversions
        mutex_(std::forward<MutexArgs>(args)...)
    {}

    std::shared_ptr<relation const> find_relation(std::string_view id) const override {
        reader_lock_type lock { mutex_ };
        return internal_find_relation(id);
    }

    /**
     * @brief provides all relations in this provider or its parent chain.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     * @note The hidden entries in the parent chain will not occur in the consumer.
     */
    void each_relation(relation_consumer_type consumer) const override {
        reader_lock_type lock { mutex_ };
        internal_each_relation(consumer);
    }

    /**
     * @brief adds a relation.
     * @param element the relation to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     * @note This operation may **hide** elements defined in the parent chain if `overwrite=true`
     */
    std::shared_ptr<relation const> add_relation(std::shared_ptr<relation const> element, bool overwrite = false) {
        writer_lock_type lock { mutex_ };
        internal_add_relation(element, overwrite);
        return element;
    }

    /// @copydoc add_relation(std::shared_ptr<relation>, bool)
    std::shared_ptr<relation> add_relation(relation&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        writer_lock_type lock { mutex_ };
        internal_add_relation(shared, overwrite);
        return shared;
    }

    /**
     * @brief adds a table.
     * @param element the table to add
     * @param overwrite true to overwrite the existing entry
     * @return the added element
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @throws std::invalid_argument if the target entry has been already owned another provider
     * @note This operation may **hide** elements defined in the parent chain if `overwrite=true`
     */
    std::shared_ptr<table> add_table(std::shared_ptr<table> element, bool overwrite = false) {
        writer_lock_type lock { mutex_ };
        internal_add_table(element, overwrite);
        return element;
    }

    /// @copydoc add_table(std::shared_ptr<table>, bool)
    std::shared_ptr<table const> add_table(std::shared_ptr<table const> element, bool overwrite = false) {
        writer_lock_type lock { mutex_ };
        internal_add_table(element, overwrite);
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
     * @note This operation may **hide** elements defined in the parent chain if `overwrite=true`
     */
    [[deprecated]]
    std::shared_ptr<relation> add_relation(std::string_view id, std::shared_ptr<relation> element, bool overwrite = false) {
        check_element_id(id, element->simple_name(), "inconsistent relation ID");
        writer_lock_type lock { mutex_ };
        internal_add_relation(element, overwrite);
        return element;
    }

    /// @copydoc add_relation(std::string_view id, std::shared_ptr<relation>, bool)
    [[deprecated]]
    std::shared_ptr<relation> add_relation(std::string_view id, relation&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        check_element_id(id, shared->simple_name(), "inconsistent relation ID");
        writer_lock_type lock { mutex_ };
        internal_add_relation(shared, overwrite);
        return shared;
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
     * @note This operation may **hide** elements defined in the parent chain if `overwrite=true`
     */
    [[deprecated]]
    std::shared_ptr<table> add_table(std::string_view id, std::shared_ptr<table> element, bool overwrite = false) {
        check_element_id(id, element->simple_name(), "inconsistent table ID");
        return add_table(std::move(element), overwrite);
    }

    /// @copydoc add_table(std::string_view id, std::shared_ptr<table>, bool)
    [[deprecated]]
    std::shared_ptr<table> add_table(std::string_view id, table&& element, bool overwrite = false) {
        auto shared = takatori::util::clone_shared(std::move(element));
        check_element_id(id, shared->simple_name(), "inconsistent table ID");
        return add_table(std::move(shared), overwrite);
    }

    /**
     * @brief removes the relation in this provider.
     * @param id the relation ID
     * @return true if successfully removed
     * @return false if there is no such a relation
     */
    bool remove_relation(std::string_view id) {
        writer_lock_type lock { mutex_ };
        return internal_remove_relation(id);
    }

    [[nodiscard]] std::shared_ptr<class index const> find_index(std::string_view id) const override {
        reader_lock_type lock { mutex_ };
        return internal_find_index(id);
    }

    /**
     * @brief provides all indices in this provider or its parent chain.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     * @note The hidden entries in the parent chain will not occur in the consumer.
     */
    void each_index(index_consumer_type const& consumer) const override {
        reader_lock_type lock { mutex_ };
        internal_each_index(consumer);
    }

    std::shared_ptr<index const> find_primary_index(class table const& table) const override {
        reader_lock_type lock { mutex_ };
        return internal_find_primary_index(table);
    }

    /**
     * @brief provides all indices of the given table in this provider or its parent chain.
     * @param table the target table
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     * @note The hidden entries in the parent chain will not occur in the consumer.
     */
    void each_table_index(class table const& table, index_consumer_type const& consumer) const override {
        reader_lock_type lock { mutex_ };
        internal_each_table_index(table, consumer);
    }

    /**
     * @brief adds an index.
     * @param element the index to add
     * @param overwrite true to overwrite the existing entry
     * @return this
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in the parent chain if `overwrite=true`
     * @attention The origin table of the index must not be changed while the index is registered in this provider.
     *      Otherwise, each_table_index() and find_primary_index() may return wrong results.
     */
    std::shared_ptr<index> add_index(std::shared_ptr<index> element, bool overwrite = false) {
        writer_lock_type lock { mutex_ };
        internal_add_index(element, overwrite);
        return element;
    }

    /// @copydoc add_index(std::shared_ptr<index>, bool)
    std::shared_ptr<index const> add_index(std::shared_ptr<index const> element, bool overwrite = false) {
        writer_lock_type lock { mutex_ };
        internal_add_index(element, overwrite);
        return element;
    }

    /// @copydoc add_index(std::shared_ptr<index>, bool)
    std::shared_ptr<index> add_index(index&& element, bool overwrite = false) {
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
     * @note This operation may **hide** elements defined in the parent chain if `overwrite=true`
     */
    [[deprecated]]
    std::shared_ptr<index> add_index(std::string_view id, std::shared_ptr<index> element, bool overwrite = false) {
        check_element_id(id, element->simple_name(), "inconsistent index ID");
        return add_index(std::move(element), overwrite);
    }

    /// @copydoc add_index(std::string_view id, std::shared_ptr<index>, bool)
    [[deprecated]]
    std::shared_ptr<index> add_index(std::string_view id, index&& element, bool overwrite = false) {
        auto shared = std::make_shared<index>(std::move(element));
        check_element_id(id, shared->simple_name(), "inconsistent index ID");
        return add_index(std::move(shared), overwrite);
    }

    /**
     * @brief removes the index in this provider.
     * @param id the index ID
     * @return true if successfully removed
     * @return false if there is no such an index
     */
    bool remove_index(std::string_view id) {
        writer_lock_type lock { mutex_ };
        return internal_remove_index(id);
    }

    [[nodiscard]] std::shared_ptr<class sequence const> find_sequence(std::string_view id) const override {
        reader_lock_type lock { mutex_ };
        return internal_find_sequence(id);
    }

    /**
     * @brief provides all sequences in this provider or its parent chain.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     * @note The hidden entries in the parent chain will not occur in the consumer.
     */
    void each_sequence(sequence_consumer_type const& consumer) const override {
        reader_lock_type lock { mutex_ };
        internal_each_sequence(consumer);
    }

    /**
     * @brief adds a sequence.
     * @param element the sequence to add
     * @param overwrite true to overwrite the existing entry
     * @return this
     * @throws std::invalid_argument if the target entry already exists and `overwrite=false`
     * @note This operation may **hide** elements defined in the parent chain if `overwrite=true`
     */
    std::shared_ptr<sequence> add_sequence(std::shared_ptr<sequence> element, bool overwrite = false) {
        writer_lock_type lock { mutex_ };
        internal_add_sequence(element, overwrite);
        return element;
    }

    /// @copydoc add_sequence(std::shared_ptr<sequence>, bool)
    std::shared_ptr<sequence const> add_sequence(std::shared_ptr<sequence const> element, bool overwrite = false) {
        writer_lock_type lock { mutex_ };
        internal_add_sequence(element, overwrite);
        return element;
    }

    /// @copydoc add_sequence(std::shared_ptr<sequence>, bool)
    std::shared_ptr<sequence> add_sequence(sequence&& element, bool overwrite = false) {
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
        writer_lock_type lock { mutex_ };
        return internal_remove_sequence(id);
    }

private:
    mutable mutex_type mutex_;
};

} // namespace yugawara::storage
