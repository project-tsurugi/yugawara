#include <yugawara/variable/comparison.h>

#include <gtest/gtest.h>

#include <takatori/value/primitive.h>
#include <takatori/util/clonable.h>

namespace yugawara::variable {

class comparison_test : public ::testing::Test {};

namespace v = takatori::value;

TEST_F(comparison_test, simple) {
    comparison expr {
            comparison_operator::equal,
            v::int4(100),
    };

    EXPECT_EQ(expr.operator_kind(), comparison_operator::equal);
    EXPECT_EQ(expr.optional_value(), v::int4(100));
}

TEST_F(comparison_test, clone) {
    comparison expr {
            comparison_operator::equal,
            v::int4(100),
    };

    auto copy = takatori::util::clone_unique(expr);
    EXPECT_EQ(expr, *copy);
    EXPECT_NE(std::addressof(expr), copy.get());
}

TEST_F(comparison_test, clone_move) {
    comparison expr {
            comparison_operator::equal,
            v::int4(100),
    };

    auto copy = takatori::util::clone_unique(expr);
    EXPECT_EQ(expr, *copy);
    EXPECT_NE(std::addressof(expr), copy.get());

    auto move = takatori::util::clone_unique(std::move(expr));
    EXPECT_NE(std::addressof(expr), move.get());
    EXPECT_EQ(*copy, *move);
}

TEST_F(comparison_test, output) {
    comparison expr {
            comparison_operator::equal,
            v::int4(100),
    };

    std::cout << expr << std::endl;
}

} // namespace yugawara::variable
