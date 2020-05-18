#pragma once

#include <memory>
#include <unordered_map>

#include <takatori/scalar/expression.h>
#include <takatori/type/data.h>
#include <takatori/util/object_creator.h>

#include "expression_resolution.h"

namespace yugawara::analyzer {

/**
 * @brief holds information of each variable binding.
 */
class expression_mapping {
public:
    /**
     * @brief creates a new instance.
     */
    expression_mapping() noexcept = default;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit expression_mapping(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief returns the resolved type for the expression.
     * @param expression the target expression
     * @return the resolved type
     * @return empty if the target expression has not been resolved yet
     */
    [[nodiscard]] expression_resolution const& find(::takatori::scalar::expression const& expression) const;

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

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const;

private:
    std::unordered_map<
            ::takatori::scalar::expression const*,
            expression_resolution,
            std::hash<::takatori::scalar::expression const*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::scalar::expression const* const,
                    expression_resolution>>> mapping_;
};

} // namespace yugawara::analyzer
