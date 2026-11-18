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
 * @brief moves the fence and the following fences downstream together to just after the given output port.
 * @details If the owner of the output port is one of the following fences (that is, the last operator of gain),
 *      it stays, and the others are placed just before it.
 * @param fence the fence
 * @param followers the following fences, in order from upstream
 * @param output the destination, which is in the downstream of the fence
 * @return the moved fences, in order from upstream
 */
[[nodiscard]] std::vector<fence_type*> move_fences(
        fence_type& fence,
        std::vector<fence_type*> const& followers,
        relation::expression::output_port_type& output) {
    std::vector<fence_type*> moved {};
    moved.reserve(followers.size() + 1);
    moved.emplace_back(std::addressof(fence));
    for (auto* follower : followers) {
        if (std::addressof(follower->output()) != std::addressof(output)) {
            moved.emplace_back(follower);
        }
    }

    // NOTE: detaches all of them first, because the destination may be next to them
    for (auto* f : moved) {
        detach_fence(*f);
    }
    auto&& anchor = output.owner();
    auto* position = std::addressof(output);
    if (fence_type::is_instance(anchor)) {
        // places the fences just before the last fence, which stays
        auto upstream = unsafe_downcast<fence_type>(anchor).input().opposite();
        if (!upstream) {
            throw_exception(std::domain_error(string_builder {}
                    << "disconnected fence: "
                    << anchor
                    << string_builder::to_string));
        }
        position = upstream.get();
    }
    // NOTE: attaches them from downstream, because each one is placed just after the position
    for (auto iter = moved.rbegin(); iter != moved.rend(); ++iter) {
        attach_fence(**iter, *position);
    }
    return moved;
}

/**
 * @brief merges the fence into the next fence, if the next operator is a fence.
 * @param graph the owner of the fence
 * @param fence the fence
 * @post if the next operator was a fence, the given fence has been removed from the graph,
 *      so that the caller must not refer it after this call
 */
void merge_fence(relation::graph_type& graph, fence_type& fence) {
    /*
     * .. - upstream - fence{a, b} - next{b, c} - ..
     * =>
     * .. - upstream - next{a, c} - ..
     */
    auto downstream = fence.output().opposite();
    if (!downstream || !fence_type::is_instance(downstream->owner())) {
        return;
    }
    auto upstream = fence.input().opposite();
    if (!upstream) {
        throw_exception(std::domain_error(string_builder {}
                << "disconnected fence: "
                << fence
                << string_builder::to_string));
    }
    auto&& next = unsafe_downcast<fence_type>(downstream->owner());
    next.upstream(fence.upstream());
    upstream->reconnect_to(next.input());
    graph.erase(fence);
}

} // namespace

void pull_up_fence(
        relation::graph_type& graph,
        remote::details::element_location const& locations) {
    // NOTE: moves the downstream fences first, so that the upstream fences can follow them.
    // Moving fences never changes the order of fences, because each fence stops at the next fence.
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
        // NOTE: if there is no destination, the next operator is not a fence, because fences are gain
        if (auto destination = find_destination(*fence, locations, followers)) {
            // NOTE: merges the moved fences in order from upstream, each into the next fence.
            // Only the moved fences and the last fence of gain can be adjacent here,
            // because a fence just after the destination would also be gain.
            for (auto* moved : move_fences(*fence, followers, *destination)) {
                merge_fence(graph, *moved);
            }
        }
    }
}

} // namespace yugawara::analyzer::details
