#include "yugawara/variable/criteria.h"

#include <gtest/gtest.h>

#include <takatori/value/int.h>

#include "yugawara/variable/comparison.h"

namespace yugawara::variable {

class comparison_test : public ::testing::Test {};

namespace v = takatori::value;

TEST_F(comparison_test, simple) {
    criteria c { ~nullable };

    EXPECT_EQ(c.nullity(), ~nullable);
    EXPECT_EQ(c.predicate(), nullptr);
}

TEST_F(comparison_test, default) {
    criteria c;

    EXPECT_EQ(c.nullity(), nullable);
    EXPECT_EQ(c.predicate(), nullptr);
}

TEST_F(comparison_test, predicate) {
    criteria c {
            ~nullable,
            comparison {
                    comparison_operator::equal,
                    v::int4(100),
            }
    };

    EXPECT_EQ(c.predicate(), (comparison {
            comparison_operator::equal,
            v::int4(100),
    }));
}

TEST_F(comparison_test, output) {
    criteria c {
            ~nullable,
            comparison {
                    comparison_operator::equal,
                    v::int4(100),
            }
    };

    std::cout << c << std::endl;
}

} // namespace yugawara::variable
