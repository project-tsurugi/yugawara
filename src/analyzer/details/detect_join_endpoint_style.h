#pragma once

#include <takatori/relation/intermediate/join.h>

namespace yugawara::analyzer::details {

enum class join_endpoint_style {
    nothing,
    key_pairs,
    prefix,
    range,
};

join_endpoint_style detect_join_endpoint_style(::takatori::relation::intermediate::join const& expr);

} // namespace yugawara::analyzer::details
