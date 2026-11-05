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
#include <takatori/relation/intermediate/union.h>

#include <takatori/util/clone_tag.h>
#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

#include "row_slice_utils.h"
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

/**
 * @brief merges the row slice of the given limit into the upstream one.
 * @details This only shrinks the row count of the upstream limit, and never removes the given limit.
 *      This never changes the sort keys of the upstream limit, that is, the sort keys of the given limit must be
 *      a prefix of the upstream ones (including empty).
 * @param expr the downstream limit
 * @param upstream the upstream limit
 * @return true if merged
 * @return false if they cannot be merged
 */
bool merge_into(relation::intermediate::limit const& expr, relation::intermediate::limit& upstream) {
    if (!upstream.group_keys().empty()) {
        return false;
    }
    // NOTE: the top rows of the upstream limit are also the top rows of the downstream one,
    // only if the downstream sort keys are a prefix of the upstream ones
    auto&& keys = expr.sort_keys();
    auto&& upstream_keys = upstream.sort_keys();
    if (keys.size() > upstream_keys.size()
            || !std::equal(keys.begin(), keys.end(), upstream_keys.begin())) {
        return false;
    }
    auto slice = merge_row_slice(expr.row_slice(), upstream.row_slice());
    if (!slice) {
        return false;
    }
    upstream.row_slice() = std::move(*slice);
    return true;
}

/**
 * @brief moves the given limit into the given input port.
 * @param limit the limit to move
 * @param destination the input port in the upstream, where the limit will be placed into
 * @return true if moved
 * @return false if the limit or the destination is not connected
 */
bool move_to(
        relation::intermediate::limit& limit,
        relation::expression::input_port_type& destination) {
    auto source = limit.input().opposite();
    auto downstream = limit.output().opposite();
    auto upstream = destination.opposite();
    if (!source || !downstream || !upstream) {
        return false;
    }
    // before: upstream -> destination .. source -> limit -> downstream
    // after:  upstream -> limit -> destination .. source -> downstream
    limit.input().disconnect_from(*source);
    limit.output().disconnect_from(*downstream);
    source->connect_to(*downstream);

    destination.disconnect_from(*upstream);
    limit.input().connect_to(*upstream);
    limit.output().connect_to(destination);
    return true;
}

optional_ptr<relation::intermediate::limit> find_upstream_limit(relation::expression::input_port_type& input) {
    if (auto upstream = input.opposite()) {
        if (auto&& owner = upstream->owner(); owner.kind() == relation::intermediate::limit::tag) {
            return unsafe_downcast<relation::intermediate::limit>(owner);
        }
    }
    return {};
}

/**
 * @brief returns the farthest upstream input port where the given limit can be moved into.
 * @param expr the target limit
 * @return the destination input port
 * @return empty if the limit cannot be moved
 */
optional_ptr<relation::expression::input_port_type> find_farthest_move_destination(
        relation::intermediate::limit& expr) {
    optional_ptr<relation::expression::input_port_type> result {};
    auto current = expr.input().opposite();
    while (current) {
        auto next = find_move_destination(expr, *current);
        if (!next) {
            break;
        }
        result = next;
        current = next->opposite();
    }
    return result;
}

class engine {
public:
    explicit engine(relation::graph_type& graph, std::size_t max_partial_limit_count) noexcept :
        graph_ { graph },
        max_partial_limit_count_ { max_partial_limit_count }
    {}

    /**
     * @brief moves the given limit to upstream, and then places a partial limit into further upstream.
     * @details This never removes any limits, and the redundant limits will be removed later
     *      (see collect_exchange_steps()).
     *      The upstream limits must be processed after the downstream ones, so that the upstream limits are moved
     *      with their row count shrunk by the downstream limits.
     * @param expr the target limit
     */
    void process(relation::intermediate::limit& expr) {
        // NOTE: moves the limit itself over the operators which make exactly one row from each input row
        if (auto destination = find_farthest_move_destination(expr)) {
            if (!move_to(expr, *destination)) {
                return;
            }
        }
        if (auto upstream_limit = find_upstream_limit(expr.input())) {
            merge_into(expr, *upstream_limit);
            return;
        }
        auto upstream = expr.input().opposite();
        if (!upstream || max_partial_limit_count_ == 0 || !is_partial_limit_target(expr)) {
            return;
        }
        // NOTE: the partial limits are placed into the upstream, and the original limit is kept in the downstream
        auto count = expr.row_slice().count().immediate_if();
        for (auto&& entry : find_partial_destinations(find_partial_entries(expr, *upstream))) {
            if (auto upstream_limit = find_upstream_limit(entry.input)) {
                // NOTE: merging into the existing limit never increases the number of exchanges
                if (merge_into(*entry.partial, *upstream_limit)) {
                    continue;
                }
            }
            if (count && *count <= max_partial_limit_count_) {
                place_partial_limit(std::move(entry.partial), entry.input);
            }
        }
    }

private:
    /**
     * @brief a partial limit and the input port where it can be placed into.
     */
    struct partial_entry {
        /// @brief the partial limit, whose sort keys refer to the columns available in the input port.
        std::unique_ptr<relation::intermediate::limit> partial;

        /// @brief the input port.
        relation::expression::input_port_type& input;
    };

    relation::graph_type& graph_;
    std::size_t max_partial_limit_count_;
    stream_variable_flow_info flow_info_ {};

    [[nodiscard]] static bool is_partial_limit_target(relation::intermediate::limit const& expr) {
        auto&& slice = expr.row_slice();
        // FIXME: currently we do not copy the limit with the effective start position (other than immediate `0`)
        if (!expr.group_keys().empty() || has_start(slice)) {
            return false;
        }
        return slice.count().immediate_if().has_value();
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

    /**
     * @brief returns a copy of the given limit whose sort keys refer to the columns of an input of the union.
     * @param source the limit in the downstream of the union
     * @param expr the union
     * @param input the input port of the union
     * @return the copy of the limit
     * @return empty if some of the sort keys are absent in the input
     */
    [[nodiscard]] static std::unique_ptr<relation::intermediate::limit> rewrite_over_union(
            relation::intermediate::limit const& source,
            relation::intermediate::union_ const& expr,
            relation::expression::input_port_type const& input) {
        bool left = std::addressof(input) == std::addressof(expr.left());
        auto&& mappings = expr.mappings();
        std::vector<relation::intermediate::limit::sort_key> sort_keys {};
        sort_keys.reserve(source.sort_keys().size());
        for (auto&& key : source.sort_keys()) {
            auto mapping = std::find_if(mappings.begin(), mappings.end(), [&](auto&& m) {
                return m.destination() == key.variable();
            });
            if (mapping == mappings.end()) {
                return {};
            }
            // NOTE: if the input does not have the column, it is always null in the rows from the input;
            // we never place the partial limit into such the input for simplicity
            auto&& column = left ? mapping->left() : mapping->right();
            if (!column) {
                return {};
            }
            sort_keys.emplace_back(*column, key.direction());
        }
        auto result = std::make_unique<relation::intermediate::limit>(::takatori::util::clone_tag, source);
        result->sort_keys() = std::move(sort_keys);
        return result;
    }

    /**
     * @brief returns the input ports of the given union, where the partial limits can be placed into.
     * @details If the union has the set quantifier `all`, each its input row becomes exactly one output row,
     *      so that the top rows of the union output only come from the top rows of each input.
     *      The sort keys of each partial limit are rewritten into the corresponding columns of the input.
     * @param source the limit in the downstream of the union
     * @param expr the union
     * @return the partial limits and their input ports, may be empty
     */
    [[nodiscard]] static std::vector<partial_entry> find_union_entries(
            relation::intermediate::limit const& source,
            relation::intermediate::union_& expr) {
        // NOTE: each input of union all has `at_least_one`, but find_path_capabilities() does not provide it yet
        // FIXME: move this into find_path_capabilities()
        std::vector<partial_entry> results {};
        if (expr.quantifier() != relation::set_quantifier::all) {
            return results;
        }
        for (auto* input : { std::addressof(expr.left()), std::addressof(expr.right()) }) {
            if (auto partial = rewrite_over_union(source, expr, *input)) {
                results.emplace_back(partial_entry { std::move(partial), *input });
            }
        }
        return results;
    }

    /**
     * @brief returns the input ports of the given upstream operator, where the partial limits can be placed into.
     * @param source the limit in the downstream of the operator
     * @param upstream the output port of the upstream operator
     * @return the copies of the limit and the input ports, whose path makes one or more rows from each input row
     * @return empty if there are no such the inputs
     */
    [[nodiscard]] std::vector<partial_entry> find_partial_entries(
            relation::intermediate::limit const& source,
            relation::expression::output_port_type& upstream) {
        auto&& owner = upstream.owner();
        if (owner.kind() == relation::intermediate::union_::tag) {
            return find_union_entries(source, unsafe_downcast<relation::intermediate::union_>(owner));
        }
        std::vector<partial_entry> results {};
        for (auto&& input : owner.input_ports()) {
            auto capabilities = find_path_capabilities(input, upstream);
            if (capabilities.contains(path_capability::at_least_one) && is_available(source, input)) {
                results.emplace_back(partial_entry {
                        std::make_unique<relation::intermediate::limit>(::takatori::util::clone_tag, source),
                        input,
                });
                break;
            }
        }
        return results;
    }

    /**
     * @brief returns the farthest upstream input ports where the partial limits can be placed into.
     * @details This walks the upstream from each given input port, while the partial limit can be moved over
     *      the operators, and stops at the upstream limit.
     *      The walk branches if the partial limit can be placed into two or more inputs of an operator
     *      (e.g. union all).
     * @param entries the partial limits and the input ports where the walk starts
     * @return the partial limits and their destination input ports
     */
    [[nodiscard]] std::vector<partial_entry> find_partial_destinations(std::vector<partial_entry> entries) {
        // NOTE: partial limits can be placed anywhere in the upstream, only if the rows in the original limit are kept
        std::vector<partial_entry> results {};
        while (!entries.empty()) {
            auto entry = std::move(entries.back());
            entries.pop_back();
            auto* current = std::addressof(entry.input);
            bool branched = false;
            while (true) {
                auto upstream = current->opposite();
                if (!upstream || upstream->owner().kind() == relation::intermediate::limit::tag) {
                    break;
                }
                if (auto next = find_move_destination(*entry.partial, *upstream)) {
                    current = next.get();
                    continue;
                }
                if (auto nexts = find_partial_entries(*entry.partial, *upstream); !nexts.empty()) {
                    for (auto&& next : nexts) {
                        entries.emplace_back(std::move(next));
                    }
                    branched = true;
                }
                break;
            }
            if (!branched) {
                results.emplace_back(partial_entry { std::move(entry.partial), *current });
            }
        }
        return results;
    }

    void place_partial_limit(
            std::unique_ptr<relation::intermediate::limit> partial,
            relation::expression::input_port_type& input) {
        auto upstream = input.opposite();
        if (!upstream) {
            return;
        }
        auto&& copy = graph_.insert(std::move(partial));
        // before: upstream -> input
        // after:  upstream -> copy -> input
        input.disconnect_from(*upstream);
        copy.input().connect_to(*upstream);
        copy.output().connect_to(input);
    }
};

} // namespace

void push_down_limits(::takatori::relation::graph_type& graph, std::size_t max_partial_limit_count) {
    std::vector<relation::intermediate::limit*> targets {};
    relation::sort_from_upstream(graph, [&](relation::expression& expr) {
        if (expr.kind() == relation::intermediate::limit::tag) {
            auto&& limit = unsafe_downcast<relation::intermediate::limit>(expr);
            if (is_target(limit)) {
                targets.emplace_back(std::addressof(limit));
            }
        }
    });
    engine e { graph, max_partial_limit_count };
    // NOTE: process from downstream to upstream, so that the upstream limits are moved after shrunk
    for (auto iter = targets.rbegin(); iter != targets.rend(); ++iter) {
        e.process(**iter);
    }
}

} // namespace yugawara::analyzer::details
