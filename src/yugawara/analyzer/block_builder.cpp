#include <yugawara/analyzer/block_builder.h>

#include <vector>

#include <tsl/hopscotch_set.h>

#include <takatori/relation/graph.h>

#include <takatori/util/assertion.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

namespace yugawara::analyzer {

namespace relation = ::takatori::relation;

using ::takatori::relation::expression_kind_set;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

using block_graph = ::takatori::graph::graph<block>;

namespace {

inline bool is_front(relation::expression const& expr) {
    static constexpr expression_kind_set force_front {
            relation::expression_kind::find,
            relation::expression_kind::scan,
            relation::expression_kind::values,
            relation::expression_kind::take_flat,
            relation::expression_kind::take_group,
            relation::expression_kind::take_cogroup,

            relation::expression_kind::join_relation,
            relation::expression_kind::aggregate_relation,
            relation::expression_kind::limit_relation,
            relation::expression_kind::union_relation,
            relation::expression_kind::intersection_relation,
            relation::expression_kind::difference_relation,
    };
    return expr.input_ports().size() != 1 || force_front.contains(expr.kind());
}

inline bool is_back(relation::expression const& expr) {
    static constexpr expression_kind_set force_back {
            relation::expression_kind::buffer,

            relation::expression_kind::emit,
            relation::expression_kind::write,

            relation::expression_kind::offer,
    };
    return expr.output_ports().size() != 1 || force_back.contains(expr.kind());
}

inline relation::expression& find_back(relation::expression& expr) {
    if (is_back(expr)) {
        return expr;
    }
    BOOST_ASSERT(expr.output_ports().size() == 1); // NOLINT
    auto&& out = expr.output_ports().front();
    if (!out.opposite()) {
        throw_exception(std::domain_error(string_builder {}
                << "invalid connectivity (must be connected): "
                << out
                << string_builder::to_string));
    }
    auto&& next = out.opposite()->owner();
    if (is_front(next)) {
        return expr;
    }
    return find_back(next);
}

} // namespace

block_builder::block_builder(block_graph blocks) noexcept
    : blocks_(std::move(blocks))
{}

block_builder::block_builder(::takatori::graph::graph<::takatori::relation::expression>& expressions)
    : blocks_(build(expressions))
{}

block_graph& block_builder::graph() noexcept {
    return blocks_;
}

block_graph const& block_builder::graph() const noexcept {
    return blocks_;
}

block_graph block_builder::release() noexcept {
    auto r = std::move(blocks_);
    blocks_ = decltype(r) {};
    return r;
}

block_graph block_builder::build(relation::graph_type& expressions) {
    std::vector<relation::expression*> heads;
    ::tsl::hopscotch_set<
            relation::expression*,
            std::hash<relation::expression*>,
            std::equal_to<>> saw;

    relation::enumerate_top(expressions, [&](relation::expression& expr) {
        heads.emplace_back(std::addressof(expr));
        saw.emplace(std::addressof(expr));
    });

    block_graph result {};
    while (!heads.empty()) {
        auto&& front = *heads.back();
        heads.pop_back();
        auto&& back = find_back(front);
        result.emplace(front, back);

        for (auto&& output : back.output_ports()) {
            if (!output.opposite()) {
                throw_exception(std::domain_error(string_builder {}
                        << "invalid connectivity (must be connected): "
                        << output
                        << string_builder::to_string));
            }
            auto&& next = output.opposite()->owner();
            if (saw.find(std::addressof(next)) == saw.end()) {
                heads.emplace_back(std::addressof(next));
                saw.emplace(std::addressof(next));
            }
        }
    }
    return result;
}

} // namespace yugawara::analyzer
