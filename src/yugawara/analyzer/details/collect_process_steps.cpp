#include "collect_process_steps.h"

#include <takatori/plan/process.h>
#include <takatori/plan/exchange.h>

#include <takatori/util/assertion.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

namespace yugawara::analyzer::details {

namespace relation = ::takatori::relation;
namespace plan = ::takatori::plan;

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

namespace {

class engine {
public:
    explicit engine(relation::graph_type& source) noexcept :
        source_ { source }
    {}

    static bool is_entry(relation::expression const& expr) {
        static constexpr relation::expression_kind_set head_operators {
                relation::expression_kind::find,
                relation::expression_kind::scan,
                relation::expression_kind::values,
                relation::expression_kind::take_flat,
                relation::expression_kind::take_group,
                relation::expression_kind::take_cogroup,
        };
        return head_operators.contains(expr.kind());
    }

    relation::graph_type operator()(relation::expression const& head) {
        BOOST_ASSERT(!relation::has_upstream(head)); // NOLINT
        work_.clear();
        do_collect(head);
        return relation::release(source_, work_);
    }

    explicit operator bool() const noexcept {
        return !source_.empty();
    }

private:
    relation::graph_type& source_;

    std::vector<relation::expression const*> work_;

    void do_collect(relation::expression const& first) {
        relation::expression const* current = std::addressof(first);
        while (true) {
            if (current->input_ports().size() >= 2) {
                throw_exception(std::domain_error(string_builder {}
                        << "invalid input: "
                        << *current
                        << string_builder::to_string));
            }
            work_.emplace_back(current);
            auto outputs = current->output_ports();
            if (outputs.empty()) {
                // no more successors
                break;
            }
            if (outputs.size() == 1) {
                // continue to the successor
                current = std::addressof(next(outputs[0]));
                continue;
            }
            // collect individual outputs
            for (auto&& p : outputs) {
                do_collect(next(p));
            }
            break;
        }
    }

    static relation::expression const& next(relation::expression::output_port_type const& output) {
        if (!output.opposite()) {
            throw_exception(std::domain_error(string_builder {}
                    << "broken connection: "
                    << output
                    << string_builder::to_string));
        }
        return output.opposite()->owner();
    }
};

} // namespace

void collect_process_steps(
        ::takatori::relation::graph_type&& source,
        ::takatori::plan::graph_type& destination) {
    engine collector { source };
    while (collector) {
        bool found = false;
        for (auto&& e : source) {
            if (engine::is_entry(e)) {
                found = true;
                [[maybe_unused]] auto&& p = destination.emplace<plan::process>(collector(e));
                BOOST_ASSERT(p.operators().contains(e)); // NOLINT
                break;
            }
        }
        if (!found) {
            throw_exception(std::domain_error("missing head operator"));
        }
    }
}

} // namespace yugawara::analyzer::details
