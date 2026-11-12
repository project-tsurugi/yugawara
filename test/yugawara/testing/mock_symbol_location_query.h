#pragma once

#include <map>
#include <stdexcept>

#include <yugawara/remote/symbol_location_query.h>

namespace yugawara::testing {

/**
 * @brief a mock of remote::symbol_location_query for testing.
 * @details This only answers the locations of the registered tables.
 *      The other symbols are considered to be defined in all available locations.
 */
class mock_symbol_location_query : public remote::symbol_location_query {
public:
    /// @brief all available locations.
    location_id_set_type all {};

    /// @brief the locations of each table.
    std::map<storage::table const*, location_id_set_type> tables {};

    [[nodiscard]] remote::location_info const& of(location_id_type) const override {
        throw std::out_of_range("not supported");
    }

    [[nodiscard]] location_id_set_type const& available_location_ids() const override {
        return all;
    }

    [[nodiscard]] location_id_set_type find(storage::table const& symbol) const override {
        return tables.at(&symbol);
    }

    [[nodiscard]] location_id_set_type find(function::declaration const&) const override {
        return all;
    }

    [[nodiscard]] location_id_set_type find(aggregate::declaration const&) const override {
        return all;
    }

    [[nodiscard]] location_id_set_type find(variable::declaration const&) const override {
        return all;
    }
};

} // namespace yugawara::testing
