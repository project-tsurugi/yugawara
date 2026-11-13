#include <yugawara/extension/relation/fence.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>

#include <takatori/util/clonable.h>

#include <yugawara/binding/factory.h>

namespace yugawara::extension::relation {

class relation_fence_test : public ::testing::Test {
public:
    binding::factory bindings;
};

static_assert(fence::tag == ::takatori::relation::intermediate::extension::tag);
static_assert(fence::extension_tag == extension_id::fence_id);

using ::takatori::util::clone_unique;

TEST_F(relation_fence_test, simple) {
    fence expr { 7, 0 };

    EXPECT_EQ(expr.upstream(), 7);
    EXPECT_EQ(expr.downstream(), 0);

    EXPECT_EQ(expr.input_ports().size(), 1);
    EXPECT_EQ(expr.output_ports().size(), 1);
    EXPECT_EQ(std::addressof(expr.input_ports()[0]), std::addressof(expr.input()));
    EXPECT_EQ(std::addressof(expr.output_ports()[0]), std::addressof(expr.output()));
    EXPECT_EQ(std::addressof(expr.input().owner()), std::addressof(expr));
    EXPECT_EQ(std::addressof(expr.output().owner()), std::addressof(expr));
}

TEST_F(relation_fence_test, setter) {
    fence expr { 7, 0 };
    expr.upstream(1).downstream(2);

    EXPECT_EQ(expr.upstream(), 1);
    EXPECT_EQ(expr.downstream(), 2);
}

TEST_F(relation_fence_test, equals) {
    fence a { 7, 0 };
    fence b { 7, 0 };
    fence c { 8, 0 };
    fence d { 7, 1 };

    EXPECT_EQ(a, b);
    EXPECT_NE(a, c);
    EXPECT_NE(a, d);

    ::takatori::relation::expression const& ea = a;
    ::takatori::relation::expression const& eb = b;
    ::takatori::relation::expression const& ec = c;
    EXPECT_EQ(ea, eb);
    EXPECT_NE(ea, ec);
}

TEST_F(relation_fence_test, clone) {
    fence expr { 7, 0 };

    auto copy = clone_unique(expr);
    EXPECT_NE(std::addressof(expr), copy.get());
    EXPECT_EQ(expr, *copy);
    EXPECT_EQ(std::addressof(copy->input().owner()), copy.get());
    EXPECT_EQ(std::addressof(copy->output().owner()), copy.get());
}

TEST_F(relation_fence_test, clone_move) {
    fence expr { 7, 0 };

    auto copy = clone_unique(std::move(expr));
    EXPECT_EQ(copy->upstream(), 7);
    EXPECT_EQ(copy->downstream(), 0);
}

TEST_F(relation_fence_test, connect) {
    auto c0 = bindings.stream_variable("c0");

    ::takatori::relation::graph_type graph;
    auto&& src = graph.insert(::takatori::relation::values {
            { c0 },
            {},
    });
    auto&& f = graph.insert(fence { 7, 0 });
    src.output() >> f.input();

    ASSERT_TRUE(f.input().opposite());
    EXPECT_EQ(std::addressof(f.input().opposite()->owner()), std::addressof(src));
}

TEST_F(relation_fence_test, output) {
    fence expr { 7, 0 };
    std::cout << expr << std::endl;
}

} // namespace yugawara::extension::relation
