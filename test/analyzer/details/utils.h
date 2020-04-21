#pragma once

#include <takatori/type/primitive.h>

#include <takatori/value/primitive.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/unary.h>
#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>

#include <takatori/relation/expression.h>
#include <takatori/relation/graph.h>
#include <takatori/relation/step/take_flat.h>
#include <takatori/relation/step/take_group.h>
#include <takatori/relation/step/take_cogroup.h>
#include <takatori/relation/step/offer.h>
#include <takatori/relation/step/flatten.h>

#include <takatori/plan/graph.h>
#include <takatori/plan/process.h>

#include <takatori/util/downcast.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>

namespace yugawara::analyzer::details {

namespace t = ::takatori::type;
namespace v = ::takatori::value;
namespace descriptor = ::takatori::descriptor;
namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::relation::step::take_flat;
using ::takatori::relation::step::take_group;
using ::takatori::relation::step::take_cogroup;
using ::takatori::relation::step::offer;
using ::takatori::relation::step::flatten;

using ::takatori::util::downcast;
using ::takatori::util::string_builder;

using varref = scalar::variable_reference;

inline scalar::immediate constant(v::int4::entity_type value = 0) {
    return scalar::immediate {
            v::int4 { value },
            t::int4  {},
    };
}

inline scalar::immediate boolean(v::boolean::entity_type value) {
    return scalar::immediate {
            v::boolean { value },
            t::boolean {},
    };
}

inline scalar::immediate unknown(v::unknown_kind value = v::unknown_kind::null) {
    return scalar::immediate {
            v::unknown { value },
            t::unknown {},
    };
}

inline scalar::unary lnot(scalar::expression&& a) {
    return scalar::unary {
            scalar::unary_operator::conditional_not,
            std::move(a),
    };
}

inline scalar::binary land(
        scalar::expression&& a,
        scalar::expression&& b) {
    return scalar::binary {
            scalar::binary_operator::conditional_and,
            std::move(a),
            std::move(b),
    };
}

inline scalar::compare compare(
        scalar::expression&& a,
        scalar::expression&& b,
        scalar::comparison_operator op = scalar::comparison_operator::equal) {
    return scalar::compare {
            op,
            std::move(a),
            std::move(b),
    };
}

inline scalar::compare compare(
        descriptor::variable a,
        descriptor::variable b,
        scalar::comparison_operator op = scalar::comparison_operator::equal) {
    return scalar::compare {
            op,
            varref { std::move(a) },
            varref { std::move(b) },
    };
}

template<class T, class Port>
inline T& next(Port& port) {
    if (!port.opposite()) {
        throw std::domain_error("not connected");
    }
    auto&& r = port.opposite()->owner();
    if (r.kind() != T::tag) {
        throw std::domain_error(string_builder {}
                << r.kind()
                << " <=> "
                << T::tag
                << string_builder::to_string);
    }
    return downcast<T>(r);
}

template<class T>
inline std::enable_if_t<std::is_base_of_v<plan::exchange, T>, T const&>
resolve(descriptor::relation const& desc) {
    auto&& info = binding::unwrap(desc);
    if (info.kind() != binding::exchange_info::tag) {
        throw std::domain_error(string_builder {}
                << info.kind()
                << " <=> "
                << binding::exchange_info::tag
                << string_builder::to_string);
    }
    auto&& decl = downcast<binding::exchange_info>(info).declaration();
    if (decl.kind() != T::tag) {
        throw std::domain_error(string_builder {}
                << decl.kind()
                << " <=> "
                << T::tag
                << string_builder::to_string);
    }
    return downcast<T>(decl);
}

inline plan::process&
find(plan::graph_type& g, relation::expression const& e) {
    for (auto&& s : g) {
        if (s.kind() == plan::step_kind::process) {
            auto&& p = downcast<plan::process>(s);
            if (p.operators().contains(e)) {
                return p;
            }
        }
    }
    throw std::domain_error(string_builder {}
            << "missing process that contain: "
            << e
            << string_builder::to_string);
}

template<class T>
std::vector<T> empty() {
    return std::vector<T>{};
}

} // namespace yugawara::analyzer::details
