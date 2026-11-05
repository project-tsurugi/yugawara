#include "row_slice_utils.h"

#include <algorithm>

namespace yugawara::analyzer::details {

using ::takatori::relation::details::row_slice;

bool has_start(row_slice const& slice) {
    auto&& start = slice.start();
    if (start.is_absent()) {
        return false;
    }
    auto position = start.immediate_if();
    return !position || *position != 0;
}

std::optional<row_slice> merge_row_slice(row_slice const& first, row_slice const& second) {
    // FIXME: currently we do not merge the row slices with the effective start position (other than immediate `0`)
    if (has_start(first) || has_start(second)) {
        return {};
    }
    auto&& first_count = first.count();
    auto&& second_count = second.count();
    if (second_count.is_absent()) {
        return row_slice { {}, first_count };
    }
    // NOTE: also accepts the same variables
    if (first_count.is_absent() || first_count == second_count) {
        return row_slice { {}, second_count };
    }
    // FIXME: currently we do not merge the different variable counts, or the variable and the immediate ones
    auto first_value = first_count.immediate_if();
    auto second_value = second_count.immediate_if();
    if (first_value && second_value) {
        return row_slice { {}, std::min(*first_value, *second_value) };
    }
    return {};
}

} // namespace yugawara::analyzer::details
