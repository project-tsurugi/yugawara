#include "detect_join_endpoint_style.h"

#include <takatori/descriptor/variable.h>

#include <takatori/scalar/variable_reference.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

using ::takatori::relation::endpoint_kind;

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

static optional_ptr<descriptor::variable const> extract_if_variable_ref(scalar::expression const& expr) {
    if (expr.kind() == scalar::variable_reference::tag) {
        return unsafe_downcast<scalar::variable_reference>(expr).variable();
    }
    return {};
}

join_endpoint_style detect_join_endpoint_style(relation::intermediate::join const& expr) {
    auto&& lower = expr.lower();
    auto&& upper = expr.upper();

    if (!expr.lower() && !expr.upper()) {
        return join_endpoint_style::nothing;
    }

    if (lower.kind() != endpoint_kind::inclusive && lower.kind() != endpoint_kind::prefixed_inclusive) {
        return join_endpoint_style::range;
    }
    if (upper.kind() != endpoint_kind::inclusive && upper.kind() != endpoint_kind::prefixed_inclusive) {
        return join_endpoint_style::range;
    }
    if (lower.keys().size() != upper.keys().size()) {
        return join_endpoint_style::range;
    }
    bool saw_not_vref = false;
    for (std::size_t i = 0, n = lower.keys().size(); i < n; ++i) {
        auto&& lk = lower.keys()[i];
        auto&& uk = upper.keys()[i];
        if (lk != uk) {
            return join_endpoint_style::range;
        }
        if (!saw_not_vref) {
            auto&& lv = extract_if_variable_ref(lk.value());
            auto&& uv = extract_if_variable_ref(uk.value());
            if (!lv || !uv) {
                saw_not_vref = true;
            }
        }
    }
    // equi join
    if (saw_not_vref) {
        return join_endpoint_style::prefix;
    }
    return join_endpoint_style::key_pairs;
}
} // namespace yugawara::analyzer::details
