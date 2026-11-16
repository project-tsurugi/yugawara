#pragma once

#include <vector>

#include <takatori/statement/statement.h>

#include <yugawara/compiler_code.h>
#include <yugawara/compiler_options.h>
#include <yugawara/diagnostic.h>

#include <yugawara/remote/location_options.h>

namespace yugawara::details {

/**
 * @brief collects restricted features from the given statement.
 * @param features the restricted features
 * @param statement the target statement
 * @param location_options the location options
 *      (restricted features about remote symbols are never detected if its symbol location query is absent)
 * @return the diagnostics by detected restricted features
 * @return empty if there are no restricted features in the statement
 */
[[nodiscard]] std::vector<diagnostic<compiler_code>> collect_restricted_features(
        restricted_feature_set features,
        ::takatori::statement::statement const& statement,
        remote::location_options location_options = {});

} // namespace yugawara::details
