#pragma once

#include <memory>
#include <vector>

#include <takatori/statement/statement.h>

#include <yugawara/compiler_code.h>
#include <yugawara/diagnostic.h>
#include <yugawara/remote/operation_location_query.h>
#include <yugawara/remote/symbol_location_query.h>
#include <yugawara/util/either.h>

namespace yugawara::details {

/**
 * @brief computes the locations where each operation in the given statement can be executed.
 * @details The locations of operations are decided by element_location.
 *      This reports an error if there are steps or statements which cannot be executed in any locations,
 *      except `execute` statements, unless `allow_empty` is enabled.
 *      The locations of `execute` statements may be empty, because their steps can be executed in different locations.
 * @param statement the target statement
 * @param query the symbol location query
 * @param allow_empty whether to allow steps and statements which cannot be executed in any locations
 * @return the operation location query for the given statement
 * @return diagnostics with compiler_code::empty_operation_location
 *      if there are steps or statements (other than `execute`) which cannot be executed in any locations
 */
[[nodiscard]] util::either<
        std::vector<diagnostic<compiler_code>>,
        std::shared_ptr<remote::operation_location_query const>>
compute_operation_locations(
        ::takatori::statement::statement const& statement,
        remote::symbol_location_query const& query,
        bool allow_empty = false);

} // namespace yugawara::details
