#pragma once

#include <memory>
#include <vector>

#include <takatori/statement/statement.h>

#include <yugawara/compiler_code.h>
#include <yugawara/diagnostic.h>
#include <yugawara/remote/operation_location_query.h>
#include <yugawara/remote/location_options.h>
#include <yugawara/util/either.h>

namespace yugawara::remote::details {

/**
 * @brief computes the locations where each operation in the given statement can be executed.
 * @details The locations of operations are decided by element_location.
 *      This reports an error if there are steps or statements which cannot be executed in any locations,
 *      except `execute` statements, unless remote::location_options::allow_empty_operation_location() is enabled.
 *      The locations of `execute` statements may be empty, because their steps can be executed in different locations.
 * @param statement the target statement
 * @param options the location options, which must have a symbol location query
 * @return the operation location query for the given statement
 * @return diagnostics with compiler_code::empty_operation_location
 *      if there are steps or statements (other than `execute`) which cannot be executed in any locations
 */
[[nodiscard]] util::either<
        std::vector<diagnostic<compiler_code>>,
        std::shared_ptr<remote::operation_location_query const>>
compute_operation_locations(
        ::takatori::statement::statement const& statement,
        remote::location_options const& options);

} // namespace yugawara::remote::details
