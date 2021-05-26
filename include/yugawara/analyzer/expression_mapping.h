#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <takatori/scalar/expression.h>
#include <takatori/type/data.h>

#include "expression_resolution.h"

namespace yugawara::analyzer {

/**
 * @brief holds information of each variable binding.
 */
class expression_mapping {
public:
    /**
     * @brief the consumer type.
     * @param expression the target expression
     * @param resolution the resolution info
     */
    using consumer_type = std::function<void(::takatori::scalar::expression const& expression, expression_resolution const& resolution)>;

    /**
     * @brief creates a new instance.
     */
    expression_mapping() noexcept = default;

    /**
     * @brief returns the resolved type for the expression.
     * @param expression the target expression
     * @return the resolved type
     * @return empty if the target expression has not been resolved yet
     */
    [[nodiscard]] expression_resolution const& find(::takatori::scalar::expression const& expression) const;

    /**
     * @brief provides all resolved entries.
     * @param consumer the entry consumer, which accepts pairs of expresion and its resolution info
     */
    void each(consumer_type const& consumer) const;

    /**
     * @brief sets the resolved type for the expression.
     * @param expression the target expression
     * @param resolution the resolved information
     * @param overwrite whether or not the overwrite the existing result if it exists
     * @return the bound type
     * @throws std::domain_error if the specified expression has been resolved with `overwrite=true`
     */
    expression_resolution const& bind(
            ::takatori::scalar::expression const& expression,
            expression_resolution resolution,
            bool overwrite = false);

    /**
     * @brief removes the resolution for the target expression.
     * @details This will do nothing if there is not resolution for the given expression.
     * @param expression the target expression
     */
    void unbind(::takatori::scalar::expression const& expression);

    /**
     * @brief removes all registered entries.
     */
    void clear() noexcept;

private:
    std::unordered_map<
            ::takatori::scalar::expression const*,
            expression_resolution,
            std::hash<::takatori::scalar::expression const*>,
            std::equal_to<>> mapping_;
};

} // namespace yugawara::analyzer
