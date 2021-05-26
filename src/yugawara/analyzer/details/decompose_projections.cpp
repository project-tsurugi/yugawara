#include "decompose_projections.h"

#include <vector>

#include <takatori/relation/project.h>
#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

void decompose_projections(relation::graph_type& graph) {
    std::vector<std::unique_ptr<relation::project>> added_ {};

    for (auto&& expr : graph) {
        if (expr.kind() != relation::project::tag) {
            continue;
        }
        auto&& prj = unsafe_downcast<relation::project>(expr);
        auto&& columns = prj.columns();
        if (columns.size() >= 2) {
            added_.reserve(added_.size() + columns.size() - 1);
            auto* last = prj.output().opposite().get();
            if (last == nullptr) {
                throw_exception(std::domain_error(string_builder {}
                        << "invalid connection: "
                        << prj.output()
                        << string_builder::to_string));
            }
            last->disconnect_all();
            while (columns.size() >= 2) {
                std::vector<relation::project::column> new_columns {};
                new_columns.reserve(1);

                auto&& last_column = columns.back();
                new_columns.emplace_back(std::move(last_column.variable()), last_column.release_value());
                columns.pop_back();

                auto&& tail = added_.emplace_back(std::make_unique<relation::project>(std::move(new_columns)));
                tail->output().connect_to(*last);
                last = std::addressof(tail->input());
            }
            BOOST_ASSERT(last != nullptr); // NOLINT
            BOOST_ASSERT(columns.size() <= 1); // NOLINT
            prj.output().connect_to(*last);
        }
    }

    graph.reserve(graph.size() + added_.size());
    for (auto&& expr : added_) {
        graph.insert(std::move(expr));
    }
}

} // namespace yugawara::analyzer::details
