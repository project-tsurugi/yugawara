#include "expand_subquery.h"

#include <memory>
#include <vector>
#include <deque>

#include <takatori/scalar/variable_reference.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/intermediate/dispatch.h>

#include <takatori/util/downcast.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/extension/relation/extension_id.h>
#include <yugawara/extension/relation/subquery.h>

#include "rewrite_stream_variables.h"

namespace yugawara::analyzer::details {

namespace {

using ::takatori::util::downcast;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;
using ::takatori::util::unsafe_downcast;

class expand_subquery_command {
public:
    expand_subquery_command() = default;
    virtual ~expand_subquery_command() = default;
    expand_subquery_command(const expand_subquery_command&) = delete;
    expand_subquery_command& operator=(const expand_subquery_command&) = delete;
    expand_subquery_command(expand_subquery_command&&) = delete;
    expand_subquery_command& operator=(expand_subquery_command&&) = delete;
};

class expand_relation_subquery_command final : public expand_subquery_command {
public:
    explicit expand_relation_subquery_command(extension::relation::subquery& target) :
        target_ { target }
    {}

    [[nodiscard]] extension::relation::subquery& target() noexcept {
        return target_;
    }

private:
    extension::relation::subquery& target_;
};


class collector {
public:
    collector() = default;

    void collect(takatori::relation::graph_type& graph) {
        add_worklist(graph);
        while (!worklist_.empty()) {
            auto* node = worklist_.front();
            worklist_.pop_front();
            collect(*node);
        }
    }

    std::vector<std::unique_ptr<expand_subquery_command>> release() noexcept {
        std::vector results { std::move(commands_) };
        commands_.clear();
        worklist_.clear();
        return results;
    }

    void operator()(::takatori::relation::expression const& expr) const {
        // NOTE: nothing to do except relation subqueries
        (void) expr;
    }

    void operator()(::takatori::relation::intermediate::extension& expr) {
        switch (expr.extension_id()) {
            case extension::relation::subquery::extension_tag:
                operator()(unsafe_downcast<extension::relation::subquery&>(expr));
                break;
            default:
                throw_exception(std::domain_error {
                    string_builder {}
                            << "unknown extension of relation expression: "
                            << "extension_id=" << expr.extension_id()
                            << string_builder::to_string
                });
        }
    }

    void operator()(extension::relation::subquery& expr) {
        commands_.emplace_back(std::make_unique<expand_relation_subquery_command>(expr));
        add_worklist(expr.query_graph());
    }

private:
    std::vector<std::unique_ptr<expand_subquery_command>> commands_ {};
    std::deque<::takatori::relation::expression*> worklist_ {};

    void add_worklist(::takatori::relation::graph_type& graph) {
        for (auto& node : graph) {
            worklist_.push_back(std::addressof(node));
        }
    }

    void collect(::takatori::relation::expression& expr) {
        ::takatori::relation::intermediate::dispatch(*this, expr);
    }
};

class processor {
public:
    void process(expand_subquery_command& command) {
        if (auto* cmd = downcast<expand_relation_subquery_command>(&command)) {
            process(*cmd);
            return;
        }
        throw_exception(std::domain_error { "unknown command for expanding subqueries" });
    }

private:
    void process(expand_relation_subquery_command& command) {
        auto&& expr = command.target();
        auto&& graph = expr.owner();

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
        auto&& escape = graph.insert(::takatori::relation::project {
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

        ::takatori::relation::merge_into(std::move(expr.query_graph()), graph);
        graph.erase(expr);
    }
};

} // namespace

void expand_subquery(takatori::relation::graph_type& graph) {
    collector e {};
    e.collect(graph);
    auto commands = e.release();
    processor p {};
    for (auto& command : commands) {
        p.process(*command);
        command = {};
    }
}

} // namespace yugawara::analyzer::details
