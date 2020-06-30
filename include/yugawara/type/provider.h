#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "declaration.h"

namespace yugawara::type {

/**
 * @brief an abstract interface of user-defined type declaration provider.
 */
class provider {
public:
    /// @brief the declaration consumer type.
    using consumer_type = std::function<void(std::shared_ptr<declaration const> const&)>;

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
     * @brief provides all type declarations into the consumer.
     * @param consumer the destination consumer
     */
    virtual void each(consumer_type const& consumer) const = 0;

    /**
     * @brief returns an type declaration.
     * @param name the type name
     * @return the corresponded type
     * @return empty if there is no such a type
     */
    [[nodiscard]] virtual std::shared_ptr<declaration const> find(std::string_view name) const = 0;
};

} // namespace yugawara::type
