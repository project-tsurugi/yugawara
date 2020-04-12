#include "step_relation_collector.h"

#include <stdexcept>
#include <cassert>

#include <takatori/relation/intermediate/escape.h>
#include <takatori/relation/step/dispatch.h>

#include <takatori/plan/process.h>
#include <takatori/plan/exchange.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/relation_info.h>
#include <yugawara/binding/exchange_info.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::util::string_builder;

namespace {

class engine {
public:
    explicit engine(plan::process& process) noexcept
        : process_(process)
    {}

    void inspect(relation::expression const& expr) {
        if (expr.kind() == relation::intermediate::escape::tag) {
            // ok.
            return;
        }
        if (!relation::is_available_in_step_plan(expr.kind())) {
            throw std::domain_error(string_builder {}
                    << "intermediate plan operator is rest: "
                    << expr
                    << string_builder::to_string);
        }
        relation::step::dispatch(*this, expr);
    }

    constexpr void operator()(relation::expression const&) noexcept {}

    void operator()(relation::join_find const& expr) {
        add_upstream(expr.source());
    }

    void operator()(relation::join_scan const& expr) {
        add_upstream(expr.source());
    }

    void operator()(relation::step::take_flat const& expr) {
        add_upstream(expr.source());
    }

    void operator()(relation::step::take_group const& expr) {
        add_upstream(expr.source());
    }

    void operator()(relation::step::take_cogroup const& expr)  {
        for (auto&& group : expr.groups()) {
            add_upstream(group.source());
        }
    }

    void operator()(relation::step::offer const& expr) {
        add_downstream(expr.destination());
    }

    void operator()(relation::find const& expr) {
        assert(!extract_exchange(expr.source())); // NOLINT
    }

    void operator()(relation::scan const& expr) {
        assert(!extract_exchange(expr.source())); // NOLINT
    }

    void operator()(relation::write const& expr) {
        assert(!extract_exchange(expr.destination())); // NOLINT
    }

private:
    plan::process& process_;

    void add_upstream(::takatori::descriptor::relation const& d) {
        if (auto exchange = extract_exchange(d)) {
            process_.add_upstream(*exchange);
        }
    }

    void add_downstream(::takatori::descriptor::relation const& d) {
        if (auto exchange = extract_exchange(d)) {
            process_.add_downstream(*exchange);
        }
    }

    ::takatori::util::optional_ptr<plan::exchange> extract_exchange(::takatori::descriptor::relation const& d) {
        auto&& info = binding::unwrap(d);
        if (info.kind() == binding::exchange_info::tag) {
            auto&& exinfo = ::takatori::util::unsafe_downcast<binding::exchange_info>(info);
            auto&& g = process_.owner();
            if (auto&& it = g.find(exinfo.declaration()); it != g.end()) {
                return ::takatori::util::unsafe_downcast<plan::exchange>(*it);
            }
            throw std::domain_error(string_builder {}
                    << "unbound exchange: "
                    << exinfo
                    << string_builder::to_string);
        }
        return {};
    }
};

} // namespace

void collect_step_relations(::takatori::plan::graph_type& destination) {
    for (auto&& s : destination) {
        if (s.kind() == plan::step_kind::process) {
            auto&& p = ::takatori::util::unsafe_downcast<plan::process>(s);
            assert(p.upstreams().empty()); // NOLINT
            assert(p.downstreams().empty()); // NOLINT
            engine e { p };
            for (auto&& r : p.operators()) {
                e.inspect(r);
            }
        }
    }
}

} // namespace yugawara::analyzer::details
