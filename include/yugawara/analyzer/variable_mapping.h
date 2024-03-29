#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <takatori/descriptor/variable.h>
#include <takatori/type/data.h>
#include <takatori/util/optional_ptr.h>

#include "variable_resolution.h"

namespace yugawara::analyzer {

/**
 * @brief holds information of each variable binding.
 */
class variable_mapping {
public:
    /**
     * @brief the consumer type.
     * @param variable the target variable
     * @param resolution the resolution info
     */
    using consumer_type = std::function<void(::takatori::descriptor::variable const& variable, variable_resolution const& resolution)>;

    /**
     * @brief creates a new instance.
     */
    variable_mapping() noexcept = default;

    /**
     * @brief returns the resolved type for the variable.
     * @param variable the target variable
     * @return the resolved type
     * @return empty if the target expression has not been resolved yet
     */
    [[nodiscard]] variable_resolution const& find(::takatori::descriptor::variable const& variable) const;

    /**
     * @brief provides all resolved entries.
     * @param consumer the entry consumer, which accepts pairs of variable descriptor and its resolution info
     */
    void each(consumer_type const& consumer) const;

    /**
     * @brief sets the resolved type for the variable.
     * @param variable the target variable
     * @param resolution the resolved information
     * @param overwrite whether or not the overwrite the existing result if it exists
     * @return the stored resolution
     * @throws std::domain_error if the specified variable has been already resolved on `overwrite=false`
     */
    variable_resolution const& bind(
            ::takatori::descriptor::variable const& variable,
            variable_resolution resolution,
            bool overwrite = false);

    /**
     * @brief removes the resolution for the target variable.
     * @details This will do nothing if there is not resolution for the given variable.
     * @param variable the target variable
     */
    void unbind(::takatori::descriptor::variable const& variable);

    /**
     * @brief removes all registered entries.
     */
    void clear() noexcept;

private:
    std::unordered_map<
            ::takatori::descriptor::variable,
            variable_resolution,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>> mapping_;
};

} // namespace yugawara::analyzer
