#pragma once

#include <yugawara/runtime_feature.h>

#include "path_capability.h"

namespace yugawara::analyzer::details {

/**
 * @brief returns the options of path capabilities for the given runtime features.
 * @param features the runtime features
 * @return the corresponding options
 */
[[nodiscard]] inline path_capability_options make_path_capability_options(runtime_feature_set const& features) noexcept {
    return path_capability_options {
            features.contains(runtime_feature::apply_preserves_order),
    };
}

} // namespace yugawara::analyzer::details
