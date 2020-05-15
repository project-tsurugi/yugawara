#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "relation.h"
#include "table.h"
#include "index.h"

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
    virtual std::shared_ptr<index const> find_primary_index(class table const& table) const;
};

} // namespace yugawara::storage
