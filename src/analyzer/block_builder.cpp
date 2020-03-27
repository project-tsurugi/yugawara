#include <yugawara/analyzer/block_builder.h>

#include <unordered_set>
#include <vector>

#include <cassert>

#include <takatori/util/string_builder.h>

namespace yugawara::analyzer {

namespace relation = ::takatori::relation;

namespace {

using ::takatori::relation::expression_kind_set;
using ::takatori::util::string_builder;

inline bool is_front(relation::expression const& expr) noexcept {
    static expression_kind_set const force_front {
            relation::expression_kind::find,
            relation::expression_kind::scan,
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

inline bool is_back(relation::expression const& expr) noexcept {
    static expression_kind_set const force_back {
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
    assert(expr.output_ports().size() == 1); // NOLINT
    auto&& out = expr.output_ports().front();
    if (!out.opposite()) {
        throw std::domain_error(string_builder {}
                << "invalid connectivity (must be connected): "
                << out
                << string_builder::to_string);
    }
    auto&& next = out.opposite()->owner();
    if (is_front(next)) {
        return expr;
    }
    return find_back(next);
}

} // namespace

block_builder::block_builder(::takatori::util::object_creator creator) noexcept
    : blocks_(creator)
{}

block_builder::block_builder(::takatori::graph::graph<block> blocks) noexcept
    : blocks_(std::move(blocks))
{}

block_builder::block_builder(::takatori::graph::graph<::takatori::relation::expression>& expressions)
    : blocks_(build(expressions))
{}

::takatori::graph::graph<block>& block_builder::graph() noexcept {
    return blocks_;
}

::takatori::graph::graph<block> const& block_builder::graph() const noexcept {
    return blocks_;
}

::takatori::graph::graph<block> block_builder::release() noexcept {
    auto r = std::move(blocks_);
    blocks_ = decltype(r) { r.get_object_creator() };
    return r;
}

::takatori::util::object_creator block_builder::get_object_creator() const noexcept {
    return blocks_.get_object_creator();
}

::takatori::graph::graph<block> block_builder::build(
        ::takatori::graph::graph<::takatori::relation::expression>& expressions) {
    std::vector<relation::expression*, ::takatori::util::object_allocator<relation::expression*>> heads;
    std::unordered_set<
            relation::expression*,
            std::hash<relation::expression*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<relation::expression*>> saw;

    // collect the "head" expressions
    for (auto&& expr : expressions) {
        if (expr.input_ports().empty()) {
            heads.emplace_back(std::addressof(expr));
            saw.emplace(std::addressof(expr));
        }
    }

    auto creator = expressions.get_object_creator();
    ::takatori::graph::graph<block> result { creator };
    while (!heads.empty()) {
        auto&& front = *heads.back();
        heads.pop_back();
        auto&& back = find_back(front);
        result.emplace(front, back, creator);

        for (auto&& output : back.output_ports()) {
            if (!output.opposite()) {
                throw std::domain_error(string_builder {}
                        << "invalid connectivity (must be connected): "
                        << output
                        << string_builder::to_string);
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
