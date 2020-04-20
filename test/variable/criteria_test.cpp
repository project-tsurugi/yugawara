#include <yugawara/variable/criteria.h>

#include <gtest/gtest.h>

#include <takatori/value/primitive.h>

#include <yugawara/variable/comparison.h>
#include <yugawara/variable/quantification.h>

namespace yugawara::variable {

class criteria_test : public ::testing::Test {};

namespace v = takatori::value;

TEST_F(criteria_test, simple) {
    criteria c { ~nullable };

    EXPECT_EQ(c.nullity(), ~nullable);
    EXPECT_EQ(c.predicate(), nullptr);
    EXPECT_EQ(c.constant(), nullptr);
}

TEST_F(criteria_test, default) {
    criteria c;

    EXPECT_EQ(c.nullity(), nullable);
    EXPECT_EQ(c.predicate(), nullptr);
}

TEST_F(criteria_test, predicate) {
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
    EXPECT_EQ(c.constant(), v::int4(100));
}

TEST_F(criteria_test, constant) {
    criteria c {
        v::int4(100)
    };

    EXPECT_EQ(c.nullity(), ~nullable);
    EXPECT_EQ(c.predicate(), (comparison {
            comparison_operator::equal,
            v::int4(100),
    }));
    EXPECT_EQ(c.constant(), v::int4(100));
}

TEST_F(criteria_test, constant_null) {
    criteria c {
            v::unknown()
    };

    EXPECT_EQ(c.predicate(), (comparison {
            comparison_operator::equal,
            v::unknown(),
    }));
    EXPECT_EQ(c.nullity(), nullable);
    EXPECT_EQ(c.constant(), v::unknown());
}

TEST_F(criteria_test, constant_not) {
    criteria c {
            nullable,
            comparison {
                comparison_operator::less,
                v::int4(100),
            }
    };

    EXPECT_EQ(c.predicate(), (comparison {
            comparison_operator::less,
            v::int4(100),
    }));
    EXPECT_EQ(c.nullity(), nullable);
    EXPECT_EQ(c.constant(), nullptr);
}

TEST_F(criteria_test, output) {
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
