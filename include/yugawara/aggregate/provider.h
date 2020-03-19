#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "declaration.h"
#include "set_quantifier.h"

namespace yugawara::aggregate {

/**
 * @brief an abstract interface of function declaration provider.
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
     * @brief provides all function declarations into the consumer.
     * @param consumer the destination consumer
     */
    virtual void each(std::function<void(std::shared_ptr<declaration const> const&)> consumer) const = 0;

    /**
     * @brief provides function declarations into the consumer.
     * @param name the target function name
     * @param quantifier the set quantifier
     * @param parameter_count the number of parameters in the target function
     * @param consumer the destination consumer
     */
    virtual void each(
            std::string_view name,
            set_quantifier quantifier,
            std::size_t parameter_count,
            std::function<void(std::shared_ptr<declaration const> const&)> consumer) const = 0;
};

} // namespace yugawara::aggregate