#pragma once

#include <takatori/relation/graph.h>

#include <yugawara/remote/details/element_location.h>

namespace yugawara::analyzer::details {

/**
 * @brief moves fences (extension::relation::fence) to downstream, to take in operators which reduce the rows.
 * @details This classifies the operators in the downstream of each fence as follows:
 *
 *      - gain: operators which reduce the rows flowing through the fence, if they are taken in
 *        (`filter` operators)
 *      - allow: operators which may be taken in, but are not worth taking in by themselves
 *        (`project` operators)
 *      - barrier: the other operators, which are never taken in
 *
 *      An operator of gain or allow becomes barrier if it cannot be executed in the upstream location of the fence,
 *      according to the given locations of elements.
 *
 *      For each fence, this follows the operators of gain or allow from the output of the fence toward downstream
 *      until it reaches an operator of barrier, and then moves the fence to just after the last operator of gain
 *      in the followed operators.
 *      That is, this rewrites `.. - fence - (gain|allow)* - gain - ..` into `.. - (gain|allow)* - gain - fence - ..`.
 *      If there are no operators of gain in the followed operators, this does not move the fence.
 *
 *      This only changes the positions of fences, and never decomposes nor reorders the other operators.
 *      Another fence is barrier.
 *      This processes fences from downstream, so that each fence can take in the operators
 *      up to the next fence, which has already been moved.
 *
 *      After deciding the destination of a fence (or its current position, if it does not move),
 *      if the next operator is another fence, this merges them into the latter one,
 *      whose upstream location becomes the one of the former: `.. - fence{a, b} - fence{b, c} - ..`
 *      becomes `.. - fence{a, c} - ..`. This keeps the merged fence even if its upstream and downstream
 *      locations are the same, and never moves it again.
 * @param graph the target graph
 * @param locations the locations of elements
 */
void pull_up_fence(
        ::takatori::relation::graph_type& graph,
        remote::details::element_location const& locations);

} // namespace yugawara::analyzer::details
