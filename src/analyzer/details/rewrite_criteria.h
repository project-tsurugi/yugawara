#pragma once

#include <takatori/relation/expression.h>
#include <takatori/relation/details/range_endpoint.h>
#include <takatori/relation/details/search_key_element.h>
#include <takatori/util/object_creator.h>
#include <takatori/util/sequence_view.h>

#include <yugawara/storage/index.h>
#include <yugawara/binding/factory.h>

#include "search_key_term.h"

namespace yugawara::analyzer::details {

template<class Parent>
void fill_search_key(
        storage::index const& index,
        std::vector<
                ::takatori::relation::details::search_key_element<Parent>,
                ::takatori::util::object_allocator<::takatori::relation::details::search_key_element<Parent>>>& keys,
        ::takatori::util::sequence_view<search_key_term*> terms,
        ::takatori::util::object_creator creator) {
    if (terms.empty()) {
        return;
    }
    binding::factory bindings { creator };
    keys.reserve(terms.size());
    for (std::size_t i = 0, n = terms.size(); i < n; ++i) {
        auto&& key = index.keys()[i];
        auto&& term = *terms[i];
        keys.emplace_back(bindings(key.column()), term.purge_equivalent_factor());
    }
}

template<class Parent>
void fill_endpoints(
        storage::index const& index,
        ::takatori::relation::details::range_endpoint<Parent, ::takatori::relation::details::search_key_element<Parent>>& lower,
        ::takatori::relation::details::range_endpoint<Parent, ::takatori::relation::details::search_key_element<Parent>>& upper,
        ::takatori::util::sequence_view<search_key_term*> terms,
        ::takatori::util::object_creator creator) {
    if (terms.empty()) {
        return;
    }
    namespace relation = ::takatori::relation;
    
    binding::factory bindings { creator };

    auto&& lower_keys = lower.keys();
    auto&& upper_keys = upper.keys();
    lower_keys.reserve(terms.size());
    upper_keys.reserve(terms.size());

    // compute other than the last term
    for (std::size_t i = 0, n = terms.size() - 1; i < n; ++i) {
        auto&& key = index.keys()[i];
        auto&& term = *terms[i];

        // FIXME: more flexible - we assume the scan key elements are "equivalent" style except the last one
        assert(term.equivalent()); // NOLINT

        lower_keys.emplace_back(bindings(key.column()), term.clone_equivalent_factor());
        upper_keys.emplace_back(bindings(key.column()), term.purge_equivalent_factor());
    }

    // process the last term
    auto&& key = index.keys()[terms.size() - 1];
    auto&& term = *terms.back();
    if (term.equivalent()) {
        lower_keys.emplace_back(bindings(key.column()), term.clone_equivalent_factor());
        upper_keys.emplace_back(bindings(key.column()), term.purge_equivalent_factor());
        lower.kind(relation::endpoint_kind::prefixed_inclusive);
        upper.kind(relation::endpoint_kind::prefixed_inclusive);
    } else {
        if (term.lower_factor()) {
            lower_keys.emplace_back(bindings(key.column()), term.purge_lower_factor());
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
            upper_keys.emplace_back(bindings(key.column()), term.purge_upper_factor());
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

} // namespace yugawara::analyzer::details
