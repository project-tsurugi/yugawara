#include "pull_up_fence.h"

#include <vector>

#include <takatori/relation/intermediate/extension.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/extension/relation/fence.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

namespace {

using fence_type = extension::relation::fence;

/**
 * @brief the classification of operators in the downstream of fences.
 * @details Operators of gain and allow must have just one input and one output, because fences move across them.
 */
enum class fence_crossing {
    /**
     * @brief operators which reduce the rows flowing through the fence, or the exchanges across locations,
     *      if they are taken in.
     */
    gain,

    /// @brief operators which may be taken in, but are not worth taking in by themselves.
    allow,

    /// @brief operators which are never taken in.
    barrier,
};

[[nodiscard]] fence_crossing classify(
        relation::expression const& expression,
        fence_type::location_id_type location,
        remote::details::element_location const& locations) {
    if (fence_type::is_instance(expression)) {
        // NOTE: taking in another fence reduces an exchange, and fences can be anywhere
        return fence_crossing::gain;
    }
    fence_crossing result {};
    switch (expression.kind()) {
        case relation::expression_kind::filter:
            result = fence_crossing::gain;
            break;
        case relation::expression_kind::project:
            result = fence_crossing::allow;
            break;
        default:
            return fence_crossing::barrier;
    }
    if (locations(expression).count(location) == 0) {
        return fence_crossing::barrier;
    }
    return result;
}

/**
 * @brief returns the destination of the fence.
 * @param fence the fence
 * @param locations the locations of elements
 * @param followers the fences in the followed operators up to the destination, in order from upstream
 * @return the output port of the last operator of gain in the downstream of the fence,
 *      that is, the fence should be placed just after it
 * @return empty if there are no such operators
 */
[[nodiscard]] optional_ptr<relation::expression::output_port_type> find_destination(
        fence_type& fence,
        remote::details::element_location const& locations,
        std::vector<fence_type*>& followers) {
    optional_ptr<relation::expression::output_port_type> destination {};
    for (auto port = fence.output().opposite(); port;) {
        auto&& current = port->owner();
        auto crossing = classify(current, fence.upstream(), locations);
        if (crossing == fence_crossing::barrier) {
            break;
        }
        // NOTE: operators of gain and allow have just one input and one output (see fence_crossing)
        BOOST_ASSERT(current.input_ports().size() == 1); // NOLINT
        BOOST_ASSERT(current.output_ports().size() == 1); // NOLINT
        auto&& output = current.output_ports()[0];
        if (crossing == fence_crossing::gain) {
            destination = output;
            if (fence_type::is_instance(current)) {
                followers.emplace_back(std::addressof(unsafe_downcast<fence_type>(current)));
            }
        }
        port = output.opposite();
    }
    return destination;
}

/**
 * @brief removes the fence from its position, and connects its upstream and downstream directly.
 * @param fence the fence
 */
void detach_fence(fence_type& fence) {
    /*
     * .. - upstream - fence - downstream - ..
     * =>
     * .. - upstream - downstream - ..
     */
    auto upstream = fence.input().opposite();
    auto downstream = fence.output().opposite();
    if (!upstream || !downstream) {
        throw_exception(std::domain_error(string_builder {}
                << "disconnected fence: "
                << fence
                << string_builder::to_string));
    }
    upstream->reconnect_to(*downstream);
}

/**
 * @brief places the detached fence just after the given output port.
 * @param fence the fence
 * @param output the output port
 */
void attach_fence(fence_type& fence, relation::expression::output_port_type& output) {
    /*
     * .. - (output) - next - ..
     * =>
     * .. - (output) - fence - next - ..
     */
    auto next = output.opposite();
    if (!next) {
        throw_exception(std::domain_error(string_builder {}
                << "disconnected port: "
                << output
                << string_builder::to_string));
    }
    output.reconnect_to(fence.input());
    fence.output().reconnect_to(*next);
}

/**
 * @brief moves the fence downstream to just after the given output port, taking in the following fences.
 * @details The fence takes over the downstream location of the last following fence, and the following fences
 *      are removed from the graph: `fence{a, b} - ... - fence{b, c} - ..` becomes `... - fence{a, c} - ..`.
 *      If the owner of the output port is the last following fence (that is, the last operator of gain),
 *      the fence is placed at the position of it.
 * @param graph the owner of the fences
 * @param fence the fence
 * @param followers the following fences up to the destination, in order from upstream
 * @param output the destination, which is in the downstream of the fence
 */
void move_fence(
        relation::graph_type& graph,
        fence_type& fence,
        std::vector<fence_type*> const& followers,
        relation::expression::output_port_type& output) {
    // NOTE: detaches the fence first, because the destination may be next to it
    detach_fence(fence);
    auto* position = std::addressof(output);
    for (auto* follower : followers) {
        if (std::addressof(follower->output()) == position) {
            // the fence will be placed at the position of the last following fence
            auto upstream = follower->input().opposite();
            if (!upstream) {
                throw_exception(std::domain_error(string_builder {}
                        << "disconnected fence: "
                        << *follower
                        << string_builder::to_string));
            }
            position = upstream.get();
        }
        fence.downstream(follower->downstream());
        detach_fence(*follower);
        graph.erase(*follower);
    }
    attach_fence(fence, *position);
}

} // namespace

void pull_up_fence(
        relation::graph_type& graph,
        remote::details::element_location const& locations) {
    // NOTE: moves the downstream fences first, so that the upstream fences can follow them.
    // Each fence only takes in the fences in its downstream, which have already been processed.
    std::vector<fence_type*> fences {};
    relation::sort_from_downstream(
            graph,
            [&](relation::expression& expression) {
                if (fence_type::is_instance(expression)) {
                    fences.emplace_back(std::addressof(unsafe_downcast<fence_type>(expression)));
                }
            });
    for (auto* fence : fences) {
        std::vector<fence_type*> followers {};
        if (auto destination = find_destination(*fence, locations, followers)) {
            move_fence(graph, *fence, followers, *destination);
        }
    }
}

} // namespace yugawara::analyzer::details
