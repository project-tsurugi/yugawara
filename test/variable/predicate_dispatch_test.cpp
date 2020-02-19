#include <yugawara/variable/dispatch.h>

#include <gtest/gtest.h>

#include <takatori/value/int.h>

namespace yugawara::variable {

class predicate_dispatch_test : public ::testing::Test {};

namespace v = takatori::value;

TEST_F(predicate_dispatch_test, switch) {
    struct cb {
        int operator()(comparison const&) { return 1; }
        int operator()(negation const&) { return 2; }
        int operator()(predicate const&) { return -1; }
    };
    comparison p1 {
            comparison_operator::equal,
            v::int4(1),
    };
    negation p2 {
            comparison {
                    comparison_operator::equal,
                    v::int4(1),
            },
    };
    quantification p3 {
            quantifier::any,
            comparison {
                    comparison_operator::equal,
                    v::int4(1),
            },
            negation {
                comparison {
                        comparison_operator::equal,
                        v::int4(1),
                },
            },
    };

    EXPECT_EQ(dispatch(cb {}, p1), 1);
    EXPECT_EQ(dispatch(cb {}, p2), 2);
    EXPECT_EQ(dispatch(cb {}, p3), -1);
}

} // namespace yugawara::variable
