#include "decompose_filter.h"

#include <takatori/scalar/binary.h>

#include <takatori/relation/filter.h>

#include <takatori/util/assertion.h>
#include <takatori/util/downcast.h>

namespace yugawara::analyzer::details {

using ::takatori::util::unsafe_downcast;

namespace scalar = ::takatori::scalar;
namespace relation = ::takatori::relation;

namespace {

class engine {
public:
    explicit engine(relation::filter& expression) :
        expression_ { expression }
    {}

    void process() {
        if (!consume(expression_.condition())) {
            return;
        }
        BOOST_ASSERT(terms_.size() >= 1); // NOLINT

        auto&& graph = expression_.owner();
        graph.reserve(graph.size() + terms_.size());
        relation::expression::input_port_type* input_port {};
        relation::expression::output_port_type* output_port {};
        for (auto&& term : terms_) {
            auto&& filter = graph.emplace<relation::filter>(std::move(term));
            if (input_port == nullptr) {
                input_port = std::addressof(filter.input());
            }
            if (output_port != nullptr) {
                filter.input().connect_to(*output_port);
            }
            output_port = std::addressof(filter.output());
        }
        terms_.clear();

        if (auto upstream = expression_.input().opposite()) {
            BOOST_ASSERT(input_port != nullptr); // NOLINT
            expression_.input().disconnect_all();
            upstream->connect_to(*input_port);
        }
        if (auto downstream = expression_.output().opposite()) {
            BOOST_ASSERT(output_port != nullptr); // NOLINT
            expression_.output().disconnect_all();
            output_port->connect_to(*downstream);
        }

        graph.erase(expression_);
    }

private:
    relation::filter& expression_;
    std::vector<std::unique_ptr<scalar::expression>> terms_ {};

    bool consume(scalar::expression& expr) {
        if (!is_conjunction(expr)) {
            return false;
        }
        auto&& binary = unsafe_downcast<scalar::binary>(expr);
        if (!consume(binary.left())) {
            terms_.emplace_back(binary.release_left());
        }
        if (!consume(binary.right())) {
            terms_.emplace_back(binary.release_right());
        }
        return true;
    }

    bool is_conjunction(scalar::expression const& expr) {
        if (expr.kind() != scalar::binary::tag) {
            return false;
        }
        auto&& binary = unsafe_downcast<scalar::binary>(expr);
        return binary.operator_kind() == scalar::binary_operator::conditional_and;
    }
};

} // namespace

void decompose_filter(relation::graph_type& graph) {
    std::vector<relation::filter*> filters {};
    for (auto&& node : graph) {
        if (node.kind() == relation::filter::tag) {
            filters.emplace_back(std::addressof(unsafe_downcast<relation::filter>(node)));
        }
    }
    for (auto* filter_ptr : filters) {
        engine e { *filter_ptr };
        e.process();
    }
}

} // namespace yugawara::analyzer::details
