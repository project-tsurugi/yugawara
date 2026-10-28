#include "push_down_limits.h"

#include <algorithm>
#include <vector>

#include <takatori/relation/project.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/downcast.h>
#include <takatori/util/optional_ptr.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::unsafe_downcast;

namespace {

bool is_target(relation::intermediate::limit const& expr) {
    return expr.group_keys().empty() && !expr.row_slice().count().is_absent();
}

bool refers(relation::intermediate::limit const& expr, descriptor::variable const& variable) {
    for (auto&& key : expr.sort_keys()) {
        if (key.variable() == variable) {
            return true;
        }
    }
    auto&& slice = expr.row_slice();
    if (auto v = slice.start().variable_if(); v && *v == variable) {
        return true;
    }
    if (auto v = slice.count().variable_if(); v && *v == variable) {
        return true;
    }
    return false;
}

bool depends_on(relation::intermediate::limit const& expr, relation::project const& project) {
    auto&& columns = project.columns();
    return std::any_of(columns.begin(), columns.end(), [&](auto&& column) {
        return refers(expr, column.variable());
    });
}

optional_ptr<relation::project> find_upstream_project(relation::intermediate::limit& expr) {
    auto upstream = expr.input().opposite();
    if (!upstream) {
        return {};
    }
    auto&& owner = upstream->owner();
    if (owner.kind() != relation::project::tag) {
        return {};
    }
    return unsafe_downcast<relation::project>(owner);
}

bool move_over(relation::intermediate::limit& limit, relation::project& project) {
    auto upstream = project.input().opposite();
    auto downstream = limit.output().opposite();
    if (!upstream || !downstream) {
        return false;
    }
    // before: upstream -> project -> limit -> downstream
    // after:  upstream -> limit -> project -> downstream
    project.input().disconnect_from(*upstream);
    project.output().disconnect_from(limit.input());
    limit.output().disconnect_from(*downstream);

    limit.input().connect_to(*upstream);
    limit.output().connect_to(project.input());
    project.output().connect_to(*downstream);
    return true;
}

void process(relation::intermediate::limit& expr) {
    while (true) {
        auto project = find_upstream_project(expr);
        if (!project || depends_on(expr, *project)) {
            return;
        }
        if (!move_over(expr, *project)) {
            return;
        }
    }
}

} // namespace

void push_down_limits(::takatori::relation::graph_type& graph) {
    std::vector<relation::intermediate::limit*> targets {};
    for (auto&& expr : graph) {
        if (expr.kind() == relation::intermediate::limit::tag) {
            auto&& limit = unsafe_downcast<relation::intermediate::limit>(expr);
            if (is_target(limit)) {
                targets.emplace_back(&limit);
            }
        }
    }
    for (auto* limit : targets) {
        process(*limit);
    }
}

} // namespace yugawara::analyzer::details
