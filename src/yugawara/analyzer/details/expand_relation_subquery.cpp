#include "expand_relation_subquery.h"

#include <vector>

#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/project.h>
#include <takatori/relation/intermediate/extension.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>

#include <yugawara/extension/relation/subquery.h>

#include "rewrite_stream_variables.h"

namespace yugawara::analyzer::details {

using ::takatori::util::unsafe_downcast;
using ::takatori::util::throw_exception;

namespace {

class engine {
public:
    explicit engine(::takatori::relation::graph_type& graph) noexcept :
        graph_ { graph }
    {}

    void process() {
        std::vector<extension::relation::subquery*> buffer {};
        while (true) {
            collect_subqueries(buffer);
            if (buffer.empty()) {
                return;
            }
            for (auto* subquery : buffer) {
                process(*subquery);
            }
            buffer.clear();
        }
    }

    void process(extension::relation::subquery& expr) {
        if (expr.is_clone()) {
            // NOTE: if this is a clone, the variables in the subquery may be shared with another subquery.
            rewrite_stream_variables(expr);
            expr.is_clone() = false;
        }

        std::vector<::takatori::relation::project::column> mappings {};
        mappings.reserve(expr.mappings().size());
        for (auto&& mapping : expr.mappings()) {
            mappings.emplace_back(
                std::make_unique<::takatori::scalar::variable_reference>(std::move(mapping.source())),
                std::move(mapping.destination())
            );
        }
        auto&& escape = graph_.insert(::takatori::relation::project {
                std::move(mappings)
        });

        auto subquery_output = expr.find_output_port();
        if (!subquery_output) {
            throw_exception(std::domain_error { "subquery has no output port" });
        }
        auto&& downstream = expr.output().opposite();
        if (!downstream) {
            throw_exception(std::domain_error { "dangling subquery" });
        }
        expr.output().disconnect_from(*downstream);

        escape.input().connect_to(*subquery_output);
        escape.output().connect_to(*downstream);

        ::takatori::relation::merge_into(std::move(expr.query_graph()), graph_);
        graph_.erase(expr);
    }

private:
    ::takatori::relation::graph_type& graph_;

    void collect_subqueries(std::vector<extension::relation::subquery*>& buffer) {
        for (auto&& expr : graph_) {
            if (expr.kind() != ::takatori::relation::intermediate::extension::tag) {
                continue;
            }
            auto& extension = unsafe_downcast<::takatori::relation::intermediate::extension>(expr);
            if (extension.extension_id() != extension::relation::subquery::extension_tag) {
                continue;
            }
            auto& subquery = unsafe_downcast<extension::relation::subquery>(extension);
            buffer.push_back(std::addressof(subquery));
        }
    }
};

} // namespace

void expand_relation_subquery(::takatori::relation::graph_type& graph) {
    engine e { graph };
    e.process();
}

} // namespace yugawara::analyzer::details
