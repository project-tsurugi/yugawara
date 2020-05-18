#pragma once

#include <functional>
#include <memory>
#include <unordered_map>

#include <takatori/descriptor/variable.h>
#include <takatori/type/data.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>

#include "variable_resolution.h"

namespace yugawara::analyzer {

/**
 * @brief holds information of each variable binding.
 */
class variable_mapping {
public:
    /**
     * @brief creates a new instance.
     */
    variable_mapping() noexcept = default;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit variable_mapping(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief returns the resolved type for the variable.
     * @param variable the target variable
     * @return the resolved type
     * @return empty if the target expression has not been resolved yet
     */
    [[nodiscard]] variable_resolution const& find(::takatori::descriptor::variable const& variable) const;

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

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const;

private:
    std::unordered_map<
            ::takatori::descriptor::variable,
            variable_resolution,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::descriptor::variable const,
                    variable_resolution>>> mapping_;
};

} // namespace yugawara::analyzer
