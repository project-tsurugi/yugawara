#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "declaration.h"

namespace yugawara::variable {

/**
 * @brief an abstract interface of external variable declaration provider.
 */
class provider {
public:
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
     * @brief provides all external variable declarations into the consumer.
     * @param consumer the destination consumer
     */
    virtual void each(std::function<void(std::shared_ptr<declaration const> const&)> consumer) const = 0;

    /**
     * @brief returns an external variable declaration.
     * @param name the variable name
     * @return the corresponded variable declaration
     * @return empty if there is no such a variable
     */
    [[nodiscard]] virtual std::shared_ptr<declaration const> find(std::string_view name) const = 0;
};

} // namespace yugawara::variable
