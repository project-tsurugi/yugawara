#include <yugawara/analyzer/block.h>

#include <gtest/gtest.h>

#include <takatori/scalar/immediate.h>
#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>

#include <takatori/relation/buffer.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/intermediate/union.h>

#include <yugawara/binding/factory.h>

namespace yugawara::analyzer {

class block_test : public ::testing::Test {
public:
    static ::takatori::scalar::immediate immediate(bool value = true) {
        return ::takatori::scalar::immediate {
                ::takatori::value::boolean { value },
                ::takatori::type::boolean {},
        };
    }

    ::takatori::descriptor::variable v() {
        return f_.stream_variable();
    }

protected:
    ::takatori::graph::graph<::takatori::relation::expression> r_;
    ::takatori::graph::graph<block> b_;
    binding::factory f_;
};

using buffer = ::takatori::relation::buffer;
using filter = ::takatori::relation::filter;
using union_ = ::takatori::relation::intermediate::union_;

TEST_F(block_test, simple) {
    auto&& o0 = r_.emplace<filter>(immediate());
    auto&& b0 = b_.emplace(o0, o0);

    EXPECT_EQ(&b0.owner(), &b_);

    EXPECT_EQ(&b0.front(), &o0);
    EXPECT_EQ(&b0.back(), &o0);

    auto iter = b0.begin();
    ASSERT_NE(iter, b0.end());
    EXPECT_EQ(&*iter, &o0);
    ++iter;
    EXPECT_EQ(iter, b0.end());
}

TEST_F(block_test, chain) {
    auto&& o0 = r_.emplace<filter>(immediate());
    auto&& o1 = r_.emplace<filter>(immediate());
    o0.output() >> o1.input();

    auto&& b0 = b_.emplace(o0, o1);
    EXPECT_EQ(&b0.front(), &o0);
    EXPECT_EQ(&b0.back(), &o1);

    auto iter = b0.begin();
    ASSERT_NE(iter, b0.end());
    EXPECT_EQ(&*iter, &o0);
    ++iter;
    ASSERT_NE(iter, b0.end());
    EXPECT_EQ(&*iter, &o1);
    ++iter;
    EXPECT_EQ(iter, b0.end());
}

TEST_F(block_test, union_buffer) {
    auto&& o0 = r_.insert(union_ {
            { v(), v(), v() },
    });
    auto&& o1 = r_.emplace<filter>(immediate());
    auto&& o2 = r_.emplace<buffer>(2);
    o0.output() >> o1.input();
    o1.output() >> o2.input();

    auto&& b0 = b_.emplace(o0, o2);
    EXPECT_EQ(&b0.front(), &o0);
    EXPECT_EQ(&b0.back(), &o2);

    auto iter = b0.begin();
    ASSERT_NE(iter, b0.end());
    EXPECT_EQ(&*iter, &o0);
    ++iter;
    ASSERT_NE(iter, b0.end());
    EXPECT_EQ(&*iter, &o1);
    ++iter;
    ASSERT_NE(iter, b0.end());
    EXPECT_EQ(&*iter, &o2);
    ++iter;
    EXPECT_EQ(iter, b0.end());
}

TEST_F(block_test, connect) {
    auto&& o0 = r_.emplace<filter>(immediate());
    auto&& o1 = r_.emplace<filter>(immediate());
    o0.output() >> o1.input();

    auto&& b0 = b_.emplace(o0, o0);
    auto&& b1 = b_.emplace(o1, o1);

    EXPECT_EQ(&b0.downstream(o0.output()), &b1);
    EXPECT_EQ(&b1.upstream(o1.input()), &b0);

    {
        auto bs = b0.downstreams();
        ASSERT_EQ(bs.size(), 1);
        EXPECT_EQ(&bs[0], &b1);
    }
    {
        auto bs = b1.upstreams();
        ASSERT_EQ(bs.size(), 1);
        EXPECT_EQ(&bs[0], &b0);
    }
}

TEST_F(block_test, union) {
    auto&& o0 = r_.emplace<filter>(immediate());
    auto&& o1 = r_.emplace<filter>(immediate());
    auto&& o2 = r_.insert(union_ {
            { v(), v(), v() },
    });
    o0.output() >> o2.left();
    o1.output() >> o2.right();

    auto&& b0 = b_.emplace(o0, o0);
    auto&& b1 = b_.emplace(o1, o1);
    auto&& b2 = b_.emplace(o2, o2);

    EXPECT_EQ(&b0.downstream(o0.output()), &b2);
    EXPECT_EQ(&b1.downstream(o1.output()), &b2);
    EXPECT_EQ(&b2.upstream(o2.left()), &b0);
    EXPECT_EQ(&b2.upstream(o2.right()), &b1);

    {
        auto bs = b0.downstreams();
        ASSERT_EQ(bs.size(), 1);
        EXPECT_EQ(&bs[0], &b2);
    }
    {
        auto bs = b1.downstreams();
        ASSERT_EQ(bs.size(), 1);
        EXPECT_EQ(&bs[0], &b2);
    }
    {
        auto bs = b2.upstreams();
        ASSERT_EQ(bs.size(), 2);
        EXPECT_EQ(&bs[0], &b0);
        EXPECT_EQ(&bs[1], &b1);
    }
}

TEST_F(block_test, buffer) {
    auto&& o0 = r_.emplace<buffer>(2);
    auto&& o1 = r_.emplace<filter>(immediate());
    auto&& o2 = r_.emplace<filter>(immediate());
    o0.output_ports()[0] >> o1.input();
    o0.output_ports()[1] >> o2.input();

    auto&& b0 = b_.emplace(o0, o0);
    auto&& b1 = b_.emplace(o1, o1);
    auto&& b2 = b_.emplace(o2, o2);

    EXPECT_EQ(&b0.downstream(o0.output_ports()[0]), &b1);
    EXPECT_EQ(&b0.downstream(o0.output_ports()[1]), &b2);
    EXPECT_EQ(&b1.upstream(o1.input()), &b0);
    EXPECT_EQ(&b2.upstream(o2.input()), &b0);

    {
        auto bs = b0.downstreams();
        ASSERT_EQ(bs.size(), 2);
        EXPECT_EQ(&bs[0], &b1);
        EXPECT_EQ(&bs[1], &b2);
    }
    {
        auto bs = b1.upstreams();
        ASSERT_EQ(bs.size(), 1);
        EXPECT_EQ(&bs[0], &b0);
    }
    {
        auto bs = b2.upstreams();
        ASSERT_EQ(bs.size(), 1);
        EXPECT_EQ(&bs[0], &b0);
    }
}

} // namespace yugawara::analyzer
