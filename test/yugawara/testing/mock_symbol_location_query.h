#pragma once

#include <map>
#include <memory>
#include <stdexcept>

#include <yugawara/remote/location_options.h>
#include <yugawara/remote/symbol_location_query.h>

namespace yugawara::testing {

/**
 * @brief a mock of remote::symbol_location_query for testing.
 * @details This only answers the locations of the registered tables.
 *      The other symbols are considered to be defined in all available locations.
 *      The location information is only available for the registered locations.
 */
class mock_symbol_location_query : public remote::symbol_location_query {
public:
    /// @brief all available locations.
    location_id_set_type all {};

    /// @brief the locations of each table.
    std::map<storage::table const*, location_id_set_type> tables {};

    /// @brief the location information of each location ID.
    std::map<location_id_type, remote::location_info> locations {};

    [[nodiscard]] remote::location_info const& of(location_id_type location_id) const override {
        return locations.at(location_id);
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

/**
 * @brief returns location options which refer the given symbol location query.
 * @details The returned object does not own the query, so that the query must outlive it.
 *      The other options are the default values.
 * @param query the symbol location query
 * @return the location options
 */
inline remote::location_options location_options_of(remote::symbol_location_query const& query) {
    remote::location_options result {};
    result.symbol_location_query() = ::takatori::util::maybe_shared_ptr { std::addressof(query) };
    return result;
}

} // namespace yugawara::testing
