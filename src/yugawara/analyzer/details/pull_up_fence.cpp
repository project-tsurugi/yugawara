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
    /// @brief operators which reduce the rows flowing through the fence, if they are taken in.
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
 * @return the output port of the last operator of gain in the downstream of the fence,
 *      that is, the fence should be placed just after it
 * @return empty if there are no such operators
 */
[[nodiscard]] optional_ptr<relation::expression::output_port_type> find_destination(
        fence_type& fence,
        remote::details::element_location const& locations) {
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
        }
        port = output.opposite();
    }
    return destination;
}

/**
 * @brief moves the fence to just after the given output port.
 * @param fence the fence
 * @param output the destination, which is in the downstream of the fence
 */
void move_fence(fence_type& fence, relation::expression::output_port_type& output) {
    /*
     * .. - upstream - fence - downstream - .. - (output) - next - ..
     * =>
     * .. - upstream - downstream - .. - (output) - fence - next - ..
     */
    auto upstream = fence.input().opposite();
    auto downstream = fence.output().opposite();
    auto next = output.opposite();
    if (!upstream || !downstream || !next) {
        throw_exception(std::domain_error(string_builder {}
                << "disconnected fence: "
                << fence
                << string_builder::to_string));
    }
    upstream->reconnect_to(*downstream);
    output.reconnect_to(fence.input());
    fence.output().reconnect_to(*next);
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
        if (auto destination = find_destination(*fence, locations)) {
            move_fence(*fence, *destination);
        }
        merge_fence(graph, *fence);
    }
}

} // namespace yugawara::analyzer::details
