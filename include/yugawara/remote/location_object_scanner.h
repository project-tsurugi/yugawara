#pragma once

#include <memory>

#include <takatori/plan/step.h>
#include <takatori/statement/statement.h>
#include <takatori/serializer/object_acceptor.h>
#include <takatori/util/maybe_shared_ptr.h>

#include <yugawara/serializer/object_scanner.h>

#include "location_info.h"
#include "operation_location_query.h"
#include "symbol_location_query.h"

namespace yugawara::remote {

/**
 * @brief an extension of serializer::object_scanner, which also scans the executable locations of operations.
 * @details This adds a property `operation_locations` to each step and statement,
 *      which is an array of location information, ordered by `location_id`:
 *
 *      - `location_id` - the location ID
 *      - `location_name` - the location name, or absent if it is not available
 *
 *      The location names are available only if the symbol location query is specified,
 *      and it provides the location information of the location IDs.
 *
 *      This only accepts the elements in the compiled statement which corresponds to the operation location query.
 *      If the operation location query is empty, this works as same as serializer::object_scanner.
 * @see compiler_result::operation_location_query()
 */
class location_object_scanner : public ::yugawara::serializer::object_scanner {
public:
    /// @brief set of numeric location IDs type.
    using location_id_set_type = location_info::location_id_set_type;

    /**
     * @brief creates a new instance.
     * @param base the base object scanner, which may contain the variable and expression mappings
     * @param operation_location_query the operation location query of the compiled statement (nullable)
     * @param symbol_location_query the symbol location query to obtain location names (nullable)
     * @see compiled_info::object_scanner()
     */
    location_object_scanner(
            ::yugawara::serializer::object_scanner base,
            std::shared_ptr<operation_location_query const> operation_location_query,
            ::takatori::util::maybe_shared_ptr<symbol_location_query const> symbol_location_query = {}) noexcept;

    using ::yugawara::serializer::object_scanner::operator();

protected:
    using ::yugawara::serializer::object_scanner::properties;

    /**
     * @brief scans properties of the given step, including its executable locations.
     * @param element the target element
     * @param acceptor the acceptor
     */
    void properties(::takatori::plan::step const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

    /**
     * @brief scans properties of the given statement, including its executable locations.
     * @param element the target element
     * @param acceptor the acceptor
     */
    void properties(::takatori::statement::statement const& element, ::takatori::serializer::object_acceptor& acceptor) const override;

private:
    std::shared_ptr<operation_location_query const> operation_location_query_;
    ::takatori::util::maybe_shared_ptr<symbol_location_query const> symbol_location_query_;

    void accept_locations(location_id_set_type const& locations, ::takatori::serializer::object_acceptor& acceptor) const;
};

} // namespace yugawara::remote
