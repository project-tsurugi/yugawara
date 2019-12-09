#include "yugawara/variable/negation.h"

#include <gtest/gtest.h>

#include <takatori/value/int.h>
#include <takatori/util/clonable.h>

#include "yugawara/variable/comparison.h"

namespace yugawara::variable {

class negation_test : public ::testing::Test {};

namespace v = takatori::value;

TEST_F(negation_test, simple) {
    negation expr {
            comparison {
                    comparison_operator::equal,
                    v::int4(100),
            }
    };

    EXPECT_EQ(expr.operand(), (comparison {
            comparison_operator::equal,
            v::int4(100),
    }));
}

TEST_F(negation_test, clone) {
    negation expr {
            comparison {
                    comparison_operator::equal,
                    v::int4(100),
            }
    };

    auto copy = takatori::util::clone_unique(expr);
    EXPECT_EQ(expr, *copy);
    EXPECT_NE(std::addressof(expr), copy.get());
}

TEST_F(negation_test, clone_move) {
    negation expr {
            comparison {
                    comparison_operator::equal,
                    v::int4(100),
            }
    };

    auto copy = takatori::util::clone_unique(expr);
    EXPECT_EQ(expr, *copy);
    EXPECT_NE(std::addressof(expr), copy.get());

    auto move = takatori::util::clone_unique(std::move(expr));
    EXPECT_NE(std::addressof(expr), move.get());
    EXPECT_EQ(*copy, *move);
}

TEST_F(negation_test, output) {
    negation expr {
            comparison {
                    comparison_operator::equal,
                    v::int4(100),
            }
    };

    std::cout << expr << std::endl;
}

} // namespace yugawara::variable
