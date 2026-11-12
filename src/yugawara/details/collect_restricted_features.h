#pragma once

#include <vector>

#include <takatori/statement/statement.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/compiler_code.h>
#include <yugawara/compiler_options.h>
#include <yugawara/diagnostic.h>

#include <yugawara/remote/symbol_location_query.h>

namespace yugawara::details {

/**
 * @brief collects restricted features from the given statement.
 * @param features the restricted features
 * @param statement the target statement
 * @param query the symbol location query, or empty if it is absent
 *      (restricted features about remote symbols are never detected if it is absent)
 * @return the diagnostics by detected restricted features
 * @return empty if there are no restricted features in the statement
 */
[[nodiscard]] std::vector<diagnostic<compiler_code>> collect_restricted_features(
        restricted_feature_set features,
        ::takatori::statement::statement const& statement,
        ::takatori::util::optional_ptr<remote::symbol_location_query const> query = {});

} // namespace yugawara::details
