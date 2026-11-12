#pragma once

#include <set>
#include <string>
#include <string_view>

#include <cstdint>

namespace yugawara::remote {

/**
 * @brief represents location information for declarations and operations.
 */
class location_info {
public:
    /// @brief numeric location ID type (available until database restart).
    using location_id_type = std::uint64_t;

    /// @brief set of numeric location IDs type.
    using location_id_set_type = std::set<location_id_type>;

    /// @brief location ID of the local server.
    static constexpr location_id_type local_location_id = 0;

    /**
     * @brief creates a new instance.
     * @param location_id the numeric ID of this location
     * @param location_name the name of this location
     */
    location_info(location_id_type location_id, std::string location_name) noexcept;

    /**
     * @brief returns the numeric ID of this location.
     * @details The location ID is consistent from the compilation to the execution of each statement,
     *      but it may change when the database restarts.
     * @return the location ID
     */
    [[nodiscard]] location_id_type location_id() const noexcept;

    /**
     * @brief returns the name of this location.
     * @return the location name
     */
    [[nodiscard]] std::string_view location_name() const noexcept;

private:
    location_id_type location_id_;
    std::string location_name_;
};

} // namespace yugawara::remote
