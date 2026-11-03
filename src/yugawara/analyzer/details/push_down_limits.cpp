#include "push_down_limits.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include <cstddef>

#include <takatori/relation/apply.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/project.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/clone_tag.h>
#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include "path_capability.h"
#include "stream_variable_flow_info.h"

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

bool depends_on(relation::intermediate::limit const& expr, relation::apply const& apply) {
    auto&& columns = apply.columns();
    return std::any_of(columns.begin(), columns.end(), [&](auto&& column) {
        return refers(expr, column.variable());
    });
}

bool depends_on(relation::intermediate::limit const& expr, relation::expression const& owner) {
    if (owner.kind() == relation::project::tag) {
        return depends_on(expr, unsafe_downcast<relation::project>(owner));
    }
    if (owner.kind() == relation::apply::tag) {
        return depends_on(expr, unsafe_downcast<relation::apply>(owner));
    }
    if (owner.kind() == relation::buffer::tag) {
        // NOTE: buffer does not define any columns
        return false;
    }
    // NOTE: the other operators may define columns referred from the limit
    return true;
}

optional_ptr<relation::expression::input_port_type> find_move_destination(
        relation::intermediate::limit const& expr,
        relation::expression::output_port_type& upstream) {
    auto&& owner = upstream.owner();
    auto&& inputs = owner.input_ports();
    if (inputs.size() != 1) {
        return {};
    }
    auto&& input = inputs[0];
    auto capabilities = find_path_capabilities(input, upstream);
    if (!capabilities.contains(path_capability::preserves_order)
            || !capabilities.contains(path_capability::at_least_one)
            || !capabilities.contains(path_capability::at_most_one)
            || depends_on(expr, owner)) {
        return {};
    }
    return input;
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

bool move_over(
        relation::intermediate::limit& limit,
        relation::expression::input_port_type& owner_input,
        relation::expression::output_port_type& owner_output) {
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

class engine {
public:
    explicit engine(relation::graph_type& graph, std::size_t max_partial_limit_count) noexcept :
        graph_ { graph },
        max_partial_limit_count_ { max_partial_limit_count }
    {}

    void process(relation::intermediate::limit& expr) {
        optional_ptr current { expr };
        // NOTE: partial limits are copies of the original limit, which is kept in the downstream
        bool partial = false;
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
                merged_.emplace_back(current.get());
                current = inner;
                partial = false;
                continue;
            }
            if (auto input = find_move_destination(*current, *upstream)) {
                if (!move_over(*current, *input, *upstream)) {
                    return;
                }
                continue;
            }
            if (auto input = find_partial_destination(*current, *upstream)) {
                if (partial) {
                    // partial limits can be placed anywhere in the upstream,
                    // only if the rows in the original limit are kept
                    if (!move_over(*current, *input, *upstream)) {
                        return;
                    }
                    continue;
                }
                auto copy = place_partial_limit(*current, *input);
                if (!copy) {
                    return;
                }
                current = copy;
                partial = true;
                continue;
            }
            return;
        }
    }

    [[nodiscard]] std::vector<relation::intermediate::limit*>& merged() noexcept {
        return merged_;
    }

private:
    relation::graph_type& graph_;
    std::size_t max_partial_limit_count_;
    stream_variable_flow_info flow_info_ {};
    std::vector<relation::intermediate::limit*> merged_ {};

    [[nodiscard]] bool is_partial_limit_target(relation::intermediate::limit const& expr) const {
        auto&& slice = expr.row_slice();
        if (!expr.group_keys().empty() || has_start(slice)) {
            return false;
        }
        auto count = slice.count().immediate_if();
        return count && *count <= max_partial_limit_count_;
    }

    [[nodiscard]] bool is_available(
            relation::intermediate::limit const& expr,
            relation::expression::input_port_type const& input) {
        auto&& owner = input.owner();
        if (owner.input_ports().size() == 1) {
            return !depends_on(expr, owner);
        }
        // NOTE: the count and start position are always immediate in partial limits
        auto&& keys = expr.sort_keys();
        return std::all_of(keys.begin(), keys.end(), [&](auto&& key) {
            return flow_info_.find(key.variable(), input).has_value();
        });
    }

    [[nodiscard]] optional_ptr<relation::expression::input_port_type> find_partial_destination(
            relation::intermediate::limit const& expr,
            relation::expression::output_port_type& upstream) {
        if (max_partial_limit_count_ == 0 || !is_partial_limit_target(expr)) {
            return {};
        }
        for (auto&& input : upstream.owner().input_ports()) {
            auto capabilities = find_path_capabilities(input, upstream);
            if (capabilities.contains(path_capability::at_least_one) && is_available(expr, input)) {
                return input;
            }
        }
        return {};
    }

    [[nodiscard]] optional_ptr<relation::intermediate::limit> place_partial_limit(
            relation::intermediate::limit const& expr,
            relation::expression::input_port_type& input) {
        auto upstream = input.opposite();
        if (!upstream) {
            return {};
        }
        auto&& copy = graph_.emplace<relation::intermediate::limit>(::takatori::util::clone_tag, expr);
        // before: upstream -> input
        // after:  upstream -> copy -> input
        input.disconnect_from(*upstream);
        copy.input().connect_to(*upstream);
        copy.output().connect_to(input);
        return copy;
    }
};

} // namespace

void push_down_limits(::takatori::relation::graph_type& graph, std::size_t max_partial_limit_count) {
    std::vector<relation::intermediate::limit*> targets {};
    for (auto&& expr : graph) {
        if (expr.kind() == relation::intermediate::limit::tag) {
            auto&& limit = unsafe_downcast<relation::intermediate::limit>(expr);
            if (is_target(limit)) {
                targets.emplace_back(std::addressof(limit));
            }
        }
    }
    engine e { graph, max_partial_limit_count };
    auto&& merged = e.merged();
    for (auto* limit : targets) {
        // NOTE: skip the limits which have already been merged into the others
        if (std::find(merged.begin(), merged.end(), limit) != merged.end()) {
            continue;
        }
        e.process(*limit);
    }
    for (auto* limit : merged) {
        graph.erase(*limit);
    }
}

} // namespace yugawara::analyzer::details
