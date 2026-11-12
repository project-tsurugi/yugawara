#pragma once

#include <yugawara/storage/table.h>
#include <yugawara/function/declaration.h>
#include <yugawara/aggregate/declaration.h>
#include <yugawara/variable/declaration.h>

#include "location_info.h"

namespace yugawara::remote {

/**
 * @brief an abstract interface to query the location information of declared symbols.
 * @details The SQL execution engine provides its implementation.
 *      The compiler uses it only during compilation.
 */
class symbol_location_query {
public:
    /// @brief numeric location ID type (available until database restart).
    using location_id_type = location_info::location_id_type;

    /// @brief set of numeric location IDs type.
    using location_id_set_type = location_info::location_id_set_type;

    /**
     * @brief creates a new instance.
     */
    constexpr symbol_location_query() = default;

    /**
     * @brief destroys this instance.
     */
    virtual ~symbol_location_query() = default;

    symbol_location_query(symbol_location_query const& other) = delete;
    symbol_location_query& operator=(symbol_location_query const& other) = delete;
    symbol_location_query(symbol_location_query&& other) noexcept = delete;
    symbol_location_query& operator=(symbol_location_query&& other) noexcept = delete;

    /**
     * @brief returns the location information of the given location ID.
     * @param location_id the location ID to query
     * @return the location information
     * @throws std::out_of_range if the given location ID is not found
     */
    [[nodiscard]] virtual location_info const& of(location_id_type location_id) const = 0;

    /**
     * @brief returns the set of all available location IDs.
     * @return the set of all available location IDs
     */
    [[nodiscard]] virtual location_id_set_type const& available_location_ids() const = 0;

    /**
     * @brief returns the set of location IDs where the given table is defined.
     * @details the returned set is a subset of available_location_ids().
     * @param symbol the table to query
     * @return the set of location IDs
     */
    [[nodiscard]] virtual location_id_set_type find(storage::table const& symbol) const = 0;

    /**
     * @brief returns the set of location IDs where the given function is defined.
     * @details the returned set is a subset of available_location_ids().
     * @param symbol the function to query
     * @return the set of location IDs
     */
    [[nodiscard]] virtual location_id_set_type find(function::declaration const& symbol) const = 0;

    /**
     * @brief returns the set of location IDs where the given aggregate function is defined.
     * @details the returned set is a subset of available_location_ids().
     * @param symbol the aggregate function to query
     * @return the set of location IDs
     */
    [[nodiscard]] virtual location_id_set_type find(aggregate::declaration const& symbol) const = 0;

    /**
     * @brief returns the set of location IDs where the given external variable is defined.
     * @details the returned set is a subset of available_location_ids().
     * @param symbol the external variable to query
     * @return the set of location IDs
     */
    [[nodiscard]] virtual location_id_set_type find(variable::declaration const& symbol) const = 0;
};

} // namespace yugawara::remote
