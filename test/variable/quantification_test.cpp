#include "yugawara/variable/quantification.h"

#include <gtest/gtest.h>

#include <takatori/value/int.h>
#include <takatori/util/clonable.h>

#include "yugawara/variable/comparison.h"

namespace yugawara::variable {

class quantification_test : public ::testing::Test {};

namespace v = takatori::value;

TEST_F(quantification_test, simple) {
    quantification expr {
            quantifier::all,
            comparison {
                    comparison_operator::greater,
                    v::int4(0),
            },
            comparison {
                    comparison_operator::less,
                    v::int4(100),
            },
    };

    EXPECT_EQ(expr.operator_kind(), quantifier::all);
    ASSERT_EQ(expr.operands().size(), 2);
    EXPECT_EQ(expr.operands()[0], (comparison {
            comparison_operator::greater,
            v::int4(0),
    }));
    EXPECT_EQ(expr.operands()[1], (comparison {
            comparison_operator::less,
            v::int4(100),
    }));
}

TEST_F(quantification_test, multiple_operands) {
    quantification expr {
            quantifier::any,
            {
                    comparison {
                            comparison_operator::equal,
                            v::int4(1),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(3),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(5),
                    },
            }
    };

    EXPECT_EQ(expr.operator_kind(), quantifier::any);
    ASSERT_EQ(expr.operands().size(), 3);
    EXPECT_EQ(expr.operands()[0], (comparison {
            comparison_operator::equal,
            v::int4(1),
    }));
    EXPECT_EQ(expr.operands()[1], (comparison {
            comparison_operator::equal,
            v::int4(3),
    }));
    EXPECT_EQ(expr.operands()[2], (comparison {
            comparison_operator::equal,
            v::int4(5),
    }));
}

TEST_F(quantification_test, clone) {
    quantification expr {
            quantifier::any,
            {
                    comparison {
                            comparison_operator::equal,
                            v::int4(1),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(3),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(5),
                    },
            }
    };

    auto copy = takatori::util::clone_unique(expr);
    EXPECT_EQ(expr, *copy);
    EXPECT_NE(std::addressof(expr), copy.get());
}

TEST_F(quantification_test, clone_move) {
    quantification expr {
            quantifier::any,
            {
                    comparison {
                            comparison_operator::equal,
                            v::int4(1),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(3),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(5),
                    },
            }
    };

    auto copy = takatori::util::clone_unique(expr);
    EXPECT_EQ(expr, *copy);
    EXPECT_NE(std::addressof(expr), copy.get());

    auto move = takatori::util::clone_unique(std::move(expr));
    EXPECT_NE(std::addressof(expr), move.get());
    EXPECT_EQ(*copy, *move);
}

TEST_F(quantification_test, output) {
    quantification expr {
            quantifier::any,
            {
                    comparison {
                            comparison_operator::equal,
                            v::int4(1),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(3),
                    },
                    comparison {
                            comparison_operator::equal,
                            v::int4(5),
                    },
            }
    };

    std::cout << expr << std::endl;
}

} // namespace yugawara::variable
