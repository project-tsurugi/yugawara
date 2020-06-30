#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "declaration.h"

namespace yugawara::schema {

/**
 * @brief an abstract interface of schema declaration provider (a.k.a. catalog).
 */
class provider {
public:
    /// @brief the declaration consumer type.
    using const_consumer_type = std::function<void(std::shared_ptr<declaration const> const&)>;

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
     * @brief provides all schema declarations into the consumer.
     * @param consumer the destination consumer
     */
    virtual void each(const_consumer_type const& consumer) const = 0;

    /**
     * @brief returns an schema declaration.
     * @param name the schema name
     * @return the corresponded schema
     * @return empty if there is no such a schema
     */
    [[nodiscard]] virtual std::shared_ptr<declaration const> find(std::string_view name) const = 0;
};

} // namespace yugawara::schema
