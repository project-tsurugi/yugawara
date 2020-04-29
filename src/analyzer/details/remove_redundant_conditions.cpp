#include "remove_redundant_conditions.h"

#include <takatori/relation/intermediate/dispatch.h>
#include <takatori/util/string_builder.h>

#include "simplify_predicate.h"

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::string_builder;

namespace {

class engine {
public:
    constexpr bool operator()(relation::expression const&) noexcept {
        return false;
    }

    bool operator()(relation::filter& expr) {
        auto r = simplify_predicate(expr.ownership_condition());
        if (r == simplify_predicate_result::constant_true) {
            auto upstream = expr.input().opposite();
            if (!upstream) {
                throw std::domain_error(string_builder {}
                        << "disconnected port: "
                        << expr.input()
                        << string_builder::to_string);
            }
            auto downstream = expr.output().opposite();
            if (!downstream) {
                throw std::domain_error(string_builder {}
                        << "disconnected port: "
                        << expr.output()
                        << string_builder::to_string);
            }
            upstream->disconnect_from(expr.input());
            downstream->disconnect_from(expr.output());
            upstream->connect_to(*downstream);
            return true;
        }
        // FIXME: more cases
        return false;
    }

    bool operator()(relation::intermediate::join& expr) {
        unset_optional_condition_if_true(expr);
        return false;
    }

    bool operator()(relation::join_find& expr) {
        unset_optional_condition_if_true(expr);
        return false;
    }

    bool operator()(relation::join_scan& expr) {
        unset_optional_condition_if_true(expr);
        return false;
    }

private:
    template<class T>
    void unset_optional_condition_if_true(T& expr) {
        if (expr.condition()) {
            auto r = simplify_predicate(expr.ownership_condition());
            if (r == simplify_predicate_result::constant_true) {
                expr.condition(nullptr);
            }
            // FIXME: more cases
        }
    }
};

} // namespace

void remove_redundant_conditions(relation::graph_type& graph) {
    engine e {};
    for (auto it = graph.begin(); it != graph.end();) {
        bool erase = relation::intermediate::dispatch(e, *it);
        if (erase) {
            it = graph.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace yugawara::analyzer::details
