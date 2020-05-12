#include "stream_variable_flow_info.h"

#include <stdexcept>

#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/assertion.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/string_builder.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;

namespace {

class engine {
public:
    using entry_type = stream_variable_flow_info::entry_type;

    explicit engine(entry_type& entry) noexcept
        : entry_(entry)
    {}

    void build(relation::expression::input_port_type const& port) {
        optional_ptr current { port };
        while (true) {
            if (!current->opposite()) {
                throw std::domain_error(string_builder {}
                        << "must be connected: "
                        << port
                        << string_builder::to_string);
            }
            auto&& expr = current->opposite()->owner();
            relation::intermediate::dispatch(*this, expr);

            if (entry_.originator()) {
                break;
            }
            BOOST_ASSERT(expr.input_ports().size() == 1); // NOLINT
            current = expr.input_ports()[0];
        }
    }

    void operator()(relation::expression const& expr) {
        throw std::domain_error(string_builder {}
                << "unsupported relation expression: "
                << expr
                << string_builder::to_string);
    }

    void operator()(relation::find const& expr) {
        declare_mappings(expr, expr.columns());
        done(expr);
    }

    void operator()(relation::scan const& expr) {
        declare_mappings(expr, expr.columns());
        done(expr);
    }

    void operator()(relation::intermediate::join const& expr) {
        // no declarations
        done(expr);
    }

    void operator()(relation::join_find const& expr) {
        declare_mappings(expr, expr.columns());
        done(expr);

        // NOTE: join_{find, scan} only requires a single input, but we consider it as an "originator"
        //       to ask "does the variable comes from left or not?".
    }

    void operator()(relation::join_scan const& expr) {
        declare_mappings(expr, expr.columns());
        done(expr);
    }

    void operator()(relation::project const& expr) {
        declare_declarators(expr, expr.columns());
        // continue
    }

    constexpr void operator()(relation::filter const&) noexcept {
        // no declarations
        // continue
    }

    constexpr void operator()(relation::buffer const&) noexcept {
        // no declarations
        // continue
    }

    void operator()(relation::intermediate::aggregate const& expr) {
        declare_mappings(expr, expr.columns());
        // continue
    }

    constexpr void operator()(relation::intermediate::distinct const&) noexcept {
        // no declarations
        // continue
    }

    constexpr void operator()(relation::intermediate::limit const&) noexcept {
        // no declarations
        // continue
    }

    void operator()(relation::intermediate::union_ const& expr) {
        declare_mappings(expr, expr.mappings());
        done(expr);
    }

    void operator()(relation::intermediate::intersection const& expr) {
        // no declarations
        done(expr);
    }

    void operator()(relation::intermediate::difference const& expr) {
        // no declarations
        done(expr);
    }

    constexpr void operator()(relation::emit const&) noexcept {
        // no declarations
        // continue
    }

    constexpr void operator()(relation::write const&) noexcept {
        // no declarations
        // continue
    }

    void operator()(relation::intermediate::escape const& expr) {
        declare_mappings(expr, expr.mappings());
        done(expr);
    }

private:
    entry_type& entry_;

    void done(relation::expression const& expr) {
        entry_.originator(expr);
    }

    template<class Mappings>
    void declare_mappings(relation::expression const& owner, Mappings& mappings) {
        for (auto&& mapping : mappings) {
            entry_.declare(mapping.destination(), owner);
        }
    }

    template<class Declarators>
    void declare_declarators(relation::expression const& owner, Declarators& declarators) {
        for (auto&& declarator : declarators) {
            entry_.declare(declarator.variable(), owner);
        }
    }
};

} // namespace

stream_variable_flow_info::entry_type& stream_variable_flow_info::entry(
        relation::expression::input_port_type const& port) {
    auto [it, success] = entries_.emplace(std::addressof(port), get_object_creator());
    if (success) {
        engine e { it->second };
        e.build(port);
    }
    return it->second;
}

optional_ptr<stream_variable_flow_info::entry_type> stream_variable_flow_info::find(
        descriptor::variable const& variable,
        relation::expression::input_port_type const& port) {
    auto&& e = entry(port);
    if (e.find(variable)) {
        return e;
    }
    BOOST_ASSERT(e.originator()); // NOLINT
    if (!e.is_separator() && e.originator()) {
        for (auto&& p : e.originator()->input_ports()) {
            if (auto r = find(variable, p)) {
                return r;
            }
        }
    }
    return {};
}

void stream_variable_flow_info::each(
        relation::expression::input_port_type const& port,
        consumer_type const& consumer) {
    auto&& e = entry(port);
    e.each(consumer);
    BOOST_ASSERT(e.originator()); // NOLINT
    if (!e.is_separator() && e.originator()) {
        for (auto&& p : e.originator()->input_ports()) {
            each(p, consumer);
        }
    }
}

} // namespace yugawara::analyzer::details
