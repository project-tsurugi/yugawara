#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "relation.h"
#include "table.h"
#include "index.h"
#include "sequence.h"

namespace yugawara::storage {

/**
 * @brief an abstract interface of storage information provider.
 */
class provider {
public:
    /**
     * @brief the consumer type.
     * @tparam the entry type
     * @param id the relation ID
     * @param entry the entry
     */
    template<class T>
    using consumer_type = std::function<void(std::string_view id, std::shared_ptr<T const> const& entry)>;

    /**
     * @brief the relation consumer type.
     */
    using relation_consumer_type = consumer_type<relation>;

    /**
     * @brief the index consumer type.
     */
    using index_consumer_type = consumer_type<index>;

    /**
     * @brief the sequence consumer type.
     */
    using sequence_consumer_type = consumer_type<sequence>;

    /**
     * @brief creates a new instance.
     */
    provider() = default;

    /**
     * @brief destroys this object.
     */
    virtual ~provider() = default;

    provider(provider const& other) = delete;
    provider& operator=(provider const& other) = delete;
    provider(provider&& other) noexcept = delete;
    provider& operator=(provider&& other) noexcept = delete;

    /**
     * @brief returns an external relation.
     * @param id the relation ID
     * @return the corresponded external relation
     * @return empty if it is absent
     */
    [[nodiscard]] virtual std::shared_ptr<relation const> find_relation(std::string_view id) const = 0;

    /**
     * @brief returns a table.
     * @param id the table ID
     * @return the corresponded table
     * @return empty if it is absent
     */
    [[nodiscard]] std::shared_ptr<class table const> find_table(std::string_view id) const;

    /**
     * @brief returns an index.
     * @param id the index ID
     * @return the corresponded index
     * @return empty if it is absent
     */
    [[nodiscard]] virtual std::shared_ptr<index const> find_index(std::string_view id) const = 0;

    /**
     * @brief returns a sequence.
     * @param id the sequence ID
     * @return the corresponded sequence
     * @return empty if it is absent
     */
    [[nodiscard]] virtual std::shared_ptr<sequence const> find_sequence(std::string_view id) const = 0;

    /**
     * @brief provides all relations in this provider.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     */
    virtual void each_relation(relation_consumer_type consumer) const = 0;

    /**
     * @brief provides all indices in this provider.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     */
    virtual void each_index(index_consumer_type const& consumer) const = 0;

    /**
     * @brief provides all indices of the given table into the consumer.
     * @param table the target table
     * @param consumer the destination consumer
     */
    virtual void each_table_index(class table const& table, index_consumer_type const& consumer) const;

    /**
     * @brief returns a primary index of the table.
     * @param table the original table
     * @return the primary index of the table
     * @return empty if it is absent
     */
    [[nodiscard]] virtual std::shared_ptr<index const> find_primary_index(class table const& table) const;

    /**
     * @brief provides all sequences in this provider.
     * @param consumer the destination consumer, which accepts pairs of element ID and element
     */
    virtual void each_sequence(sequence_consumer_type const& consumer) const = 0;

protected:
    /**
     * @brief set this as the given relation's owner.
     * @details Subclasses should call this function every relation was registered.
     * @param element the target relation
     * @throws std::invalid_argument if the target relation has been already another provider
     * @see relation::owner()
     */
    void bless(relation const& element);

    /**
     * @brief clears the given relation's owner.
     * @details Subclasses should call this function every relation goes to unregister.
     * @param element the target relation
     * @throws std::invalid_argument if this does not own the given relation
     * @see relation::owner()
     */
    void unbless(relation const& element);
    
    /**
     * @brief set this as the given sequence's owner.
     * @details Subclasses should call this function every sequence was registered.
     * @param element the target sequence
     * @throws std::invalid_argument if the target sequence has been already another provider
     * @see sequence::owner()
     */
    void bless(sequence const& element);

    /**
     * @brief clears the given sequence's owner.
     * @details Subclasses should call this function every sequence goes to unregister.
     * @param element the target sequence
     * @throws std::invalid_argument if this does not own the given sequence
     * @see sequence::owner()
     */
    void unbless(sequence const& element);
};

} // namespace yugawara::storage
