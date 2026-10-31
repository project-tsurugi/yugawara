#include "push_down_limits.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include <takatori/relation/project.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include "path_capability.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

namespace {

bool is_target(relation::intermediate::limit const& expr) {
    return expr.group_keys().empty() && !expr.row_slice().count().is_absent();
}

bool refers(relation::intermediate::limit const& expr, descriptor::variable const& variable) {
    for (auto&& key : expr.sort_keys()) {
        if (key.variable() == variable) {
            return true;
        }
    }
    auto&& slice = expr.row_slice();
    if (auto v = slice.start().variable_if(); v && *v == variable) {
        return true;
    }
    if (auto v = slice.count().variable_if(); v && *v == variable) {
        return true;
    }
    return false;
}

bool depends_on(relation::intermediate::limit const& expr, relation::project const& project) {
    auto&& columns = project.columns();
    return std::any_of(columns.begin(), columns.end(), [&](auto&& column) {
        return refers(expr, column.variable());
    });
}

bool depends_on(relation::intermediate::limit const& expr, relation::expression const& owner) {
    if (owner.kind() == relation::project::tag) {
        return depends_on(expr, unsafe_downcast<relation::project>(owner));
    }
    // NOTE: the other operators may define columns referred from the limit
    return true;
}

bool is_movable_over(relation::expression::output_port_type const& upstream) {
    auto&& inputs = upstream.owner().input_ports();
    if (inputs.size() != 1) {
        return false;
    }
    auto capabilities = find_path_capabilities(inputs[0], upstream);
    return capabilities.contains(path_capability::preserves_order)
        && capabilities.contains(path_capability::exactly_one);
}

bool has_start(relation::intermediate::limit::row_slice_type const& slice) {
    auto&& start = slice.start();
    if (start.is_absent()) {
        return false;
    }
    auto position = start.immediate_if();
    return !position || *position != 0;
}

std::optional<relation::intermediate::limit::row_slice_type> merge_row_slice(
        relation::intermediate::limit::row_slice_type const& outer,
        relation::intermediate::limit::row_slice_type const& inner) {
    // FIXME: currently we do not merge the row slices with the effective start position (other than immediate `0`)
    if (has_start(outer) || has_start(inner)) {
        return {};
    }
    auto&& outer_count = outer.count();
    auto&& inner_count = inner.count();
    if (inner_count.is_absent()) {
        return relation::intermediate::limit::row_slice_type { {}, outer_count };
    }
    // NOTE: also accepts the same variables
    if (outer_count.is_absent() || outer_count == inner_count) {
        return relation::intermediate::limit::row_slice_type { {}, inner_count };
    }
    // FIXME: currently we do not merge the different variable counts, or the variable and the immediate ones
    auto outer_value = outer_count.immediate_if();
    auto inner_value = inner_count.immediate_if();
    if (outer_value && inner_value) {
        return relation::intermediate::limit::row_slice_type { {}, std::min(*outer_value, *inner_value) };
    }
    return {};
}

bool merge_into(relation::intermediate::limit& outer, relation::intermediate::limit& inner) {
    if (!inner.group_keys().empty()) {
        return false;
    }
    // NOTE: the top rows of the inner limit are also the top rows of the outer one,
    // only if the outer sort keys are a prefix of the inner ones
    auto&& outer_keys = outer.sort_keys();
    auto&& inner_keys = inner.sort_keys();
    if (outer_keys.size() > inner_keys.size()
            || !std::equal(outer_keys.begin(), outer_keys.end(), inner_keys.begin())) {
        return false;
    }
    auto slice = merge_row_slice(outer.row_slice(), inner.row_slice());
    if (!slice) {
        return false;
    }
    auto downstream = outer.output().opposite();
    if (!downstream) {
        return false;
    }
    // before: inner -> outer -> downstream
    // after:  inner -> downstream
    inner.row_slice() = std::move(*slice);
    outer.output().disconnect_from(*downstream);
    inner.output().disconnect_from(outer.input());
    inner.output().connect_to(*downstream);
    return true;
}

bool move_over(relation::intermediate::limit& limit, relation::expression::output_port_type& owner_output) {
    auto&& owner_input = owner_output.owner().input_ports()[0];
    auto upstream = owner_input.opposite();
    auto downstream = limit.output().opposite();
    if (!upstream || !downstream) {
        return false;
    }
    // before: upstream -> operator -> limit -> downstream
    // after:  upstream -> limit -> operator -> downstream
    owner_input.disconnect_from(*upstream);
    owner_output.disconnect_from(limit.input());
    limit.output().disconnect_from(*downstream);

    limit.input().connect_to(*upstream);
    limit.output().connect_to(owner_input);
    owner_output.connect_to(*downstream);
    return true;
}

void process(relation::intermediate::limit& expr, std::vector<relation::intermediate::limit*>& merged) {
    optional_ptr current { expr };
    while (true) {
        auto upstream = current->input().opposite();
        if (!upstream) {
            return;
        }
        auto&& owner = upstream->owner();
        if (owner.kind() == relation::intermediate::limit::tag) {
            auto&& inner = unsafe_downcast<relation::intermediate::limit>(owner);
            if (!merge_into(*current, inner)) {
                return;
            }
            merged.emplace_back(current.get());
            current = inner;
            continue;
        }
        if (is_movable_over(*upstream) && !depends_on(*current, owner)) {
            if (!move_over(*current, *upstream)) {
                return;
            }
            continue;
        }
        return;
    }
}

} // namespace

void push_down_limits(::takatori::relation::graph_type& graph) {
    std::vector<relation::intermediate::limit*> targets {};
    for (auto&& expr : graph) {
        if (expr.kind() == relation::intermediate::limit::tag) {
            auto&& limit = unsafe_downcast<relation::intermediate::limit>(expr);
            if (is_target(limit)) {
                targets.emplace_back(std::addressof(limit));
            }
        }
    }
    std::vector<relation::intermediate::limit*> merged {};
    for (auto* limit : targets) {
        // NOTE: skip the limits which have already been merged into the others
        if (std::find(merged.begin(), merged.end(), limit) != merged.end()) {
            continue;
        }
        process(*limit, merged);
    }
    for (auto* limit : merged) {
        graph.erase(*limit);
    }
}

} // namespace yugawara::analyzer::details
