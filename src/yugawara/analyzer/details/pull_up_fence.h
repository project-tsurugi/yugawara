#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/remote/details/element_location.h>

namespace yugawara::analyzer::details {

/**
 * @brief moves fences (extension::relation::fence) to downstream, to take in operators which reduce the rows
 *      or the exchanges across locations.
 * @details This classifies the operators in the downstream of each fence as follows:
 *
 *      - gain: operators which reduce the rows flowing through the fence, or the exchanges across locations,
 *        if they are taken in (`filter` operators and other fences)
 *      - allow: operators which may be taken in, but are not worth taking in by themselves
 *        (`project` operators)
 *      - barrier: the other operators, which are never taken in
 *
 *      An operator of gain or allow, except fences, becomes barrier if it cannot be executed
 *      in the upstream location of the fence, according to the given locations of elements.
 *
 *      For each fence, this follows the operators of gain or allow from the output of the fence toward downstream
 *      until it reaches an operator of barrier, and then moves the fence to just after the last operator of gain
 *      in the followed operators.
 *      That is, this rewrites `.. - fence - (gain|allow)* - gain - ..` into `.. - (gain|allow)* - gain - fence - ..`.
 *      If there are no operators of gain in the followed operators, this does not move the fence.
 *
 *      If the followed operators contain other fences, the fence takes them in: it takes over the downstream
 *      location of the last one, and they are removed from the graph. That is,
 *      `.. - fence{a, b} - ... - fence{b, c} - ..` becomes `.. - ... - fence{a, c} - ..`.
 *      If the last operator of gain is a fence, the fence is placed at the position of it.
 *      If the upstream and downstream locations of the fence become the same, it no longer switches the locations,
 *      so that this removes it from the graph instead of placing it.
 *
 *      This only changes the positions of fences, and never decomposes nor reorders the other operators.
 *      This processes fences from downstream, so that each fence can take in the operators
 *      beyond the next fence, which has already been moved.
 * @param graph the target graph
 * @param locations the locations of elements
 */
void pull_up_fence(
        ::takatori::relation::graph_type& graph,
        remote::details::element_location const& locations);

} // namespace yugawara::analyzer::details
