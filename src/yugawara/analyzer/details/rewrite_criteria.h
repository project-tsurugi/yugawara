#pragma once

#include <takatori/relation/expression.h>
#include <takatori/relation/details/range_endpoint.h>
#include <takatori/relation/details/search_key_element.h>
#include <takatori/util/assertion.h>
#include <takatori/util/sequence_view.h>

#include <yugawara/storage/index.h>
#include <yugawara/binding/factory.h>

#include "search_key_term.h"

namespace yugawara::analyzer::details {

template<class Parent>
void fill_search_key(
        storage::index const& index,
        std::vector<::takatori::relation::details::search_key_element<Parent>>& keys,
        ::takatori::util::sequence_view<search_key_term*> terms) {
    if (terms.empty()) {
        return;
    }
    binding::factory bindings {};
    keys.reserve(terms.size());
    for (std::size_t i = 0, n = terms.size(); i < n; ++i) {
        auto&& key = index.keys()[i];
        auto&& term = *terms[i];
        keys.emplace_back(bindings(key.column()), term.purge_equivalent_factor());
    }
}

template<class Parent>
void add_endpoint_term(
        ::takatori::relation::details::range_endpoint<Parent, ::takatori::relation::details::search_key_element<Parent>>& lower,
        ::takatori::relation::details::range_endpoint<Parent, ::takatori::relation::details::search_key_element<Parent>>& upper,
        ::takatori::descriptor::variable const& key,
        search_key_term& term,
        bool last) {
    namespace relation = ::takatori::relation;

    auto&& lower_keys = lower.keys();
    auto&& upper_keys = upper.keys();

    if (!last) {
        // FIXME: more flexible - we assume the scan key elements are "equivalent" style except the last one
        BOOST_ASSERT(term.equivalent()); // NOLINT

        lower_keys.emplace_back(key, term.clone_equivalent_factor());
        upper_keys.emplace_back(key, term.purge_equivalent_factor());
        return;
    }

    if (term.equivalent()) {
        lower_keys.emplace_back(key, term.clone_equivalent_factor());
        upper_keys.emplace_back(key, term.purge_equivalent_factor());
        lower.kind(relation::endpoint_kind::prefixed_inclusive);
        upper.kind(relation::endpoint_kind::prefixed_inclusive);
    } else {
        if (term.lower_factor()) {
            lower_keys.emplace_back(key, term.purge_lower_factor());
            if (term.lower_inclusive()) {
                lower.kind(relation::endpoint_kind::prefixed_inclusive);
            } else {
                lower.kind(relation::endpoint_kind::prefixed_exclusive);
            }
        } else {
            if (!lower_keys.empty()) {
                lower.kind(relation::endpoint_kind::prefixed_inclusive);
            }
        }
        if (term.upper_factor()) {
            upper_keys.emplace_back(key, term.purge_upper_factor());
            if (term.upper_inclusive()) {
                upper.kind(relation::endpoint_kind::prefixed_inclusive);
            } else {
                upper.kind(relation::endpoint_kind::prefixed_exclusive);
            }
        } else {
            if (!upper_keys.empty()) {
                upper.kind(relation::endpoint_kind::prefixed_inclusive);
            }
        }
    }
}

template<class Parent>
void fill_endpoints(
        storage::index const& index,
        ::takatori::relation::details::range_endpoint<Parent, ::takatori::relation::details::search_key_element<Parent>>& lower,
        ::takatori::relation::details::range_endpoint<Parent, ::takatori::relation::details::search_key_element<Parent>>& upper,
        ::takatori::util::sequence_view<search_key_term*> terms) {
    if (terms.empty()) {
        return;
    }
    lower.keys().reserve(terms.size());
    upper.keys().reserve(terms.size());

    binding::factory bindings {};
    for (std::size_t i = 0, n = terms.size(); i < n; ++i) {
        auto&& key = index.keys()[i];
        auto&& term = *terms[i];
        bool last = (i == n - 1);
        add_endpoint_term(lower, upper, bindings(key.column()), term, last);
    }
}

inline void swap_upstream(
        ::takatori::relation::expression::input_port_type& a,
        ::takatori::relation::expression::input_port_type& b) {
    // swap left <=> right upstreams
    b.connect_to(*a.reconnect_to(*b.opposite()));
}

} // namespace yugawara::analyzer::details
