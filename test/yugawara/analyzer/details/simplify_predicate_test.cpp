#include <yugawara/analyzer/details/simplify_predicate.h>

#include <gtest/gtest.h>

#include <takatori/scalar/binary.h>
#include <takatori/scalar/unary.h>
#include <takatori/util/clonable.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::scalar::unary;
using ::takatori::scalar::binary;
using ::takatori::scalar::unary_operator;
using ::takatori::scalar::binary_operator;

using ::takatori::util::object_ownership_reference;
using ::takatori::util::unique_object_ptr;
using ::takatori::util::clone_unique;

using details::simplify_predicate;
using details::simplify_predicate_result;

class simplify_predicate_test : public ::testing::Test {
public:
    static object_ownership_reference<scalar::expression> make_ref(unique_object_ptr<scalar::expression>& expr) {
        return object_ownership_reference<scalar::expression> { expr };
    }
};

static scalar::immediate not_sure() {
    return constant();
}

static scalar::binary true_or_unknown() {
    return scalar::binary {
            scalar::binary_operator::conditional_or,
            unknown(),
            not_sure(),
    };
}

static scalar::binary false_or_unknown() {
    return scalar::binary {
            scalar::binary_operator::conditional_and,
            unknown(),
            not_sure(),
    };
}

TEST_F(simplify_predicate_test, true) {
    auto orig = boolean(true);
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, false) {
    auto orig = boolean(false);
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, unknown) {
    auto orig = unknown();
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, vary) {
    auto orig = not_sure();
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, true_or_unknown) {
    auto orig = true_or_unknown();
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true_or_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, false_or_unknown) {
    auto orig = false_or_unknown();
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false_or_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, not_true) {
    scalar::unary orig {
            scalar::unary_operator::conditional_not,
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, not_false) {
    scalar::unary orig {
            scalar::unary_operator::conditional_not,
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, not_unknown) {
    scalar::unary orig {
            scalar::unary_operator::conditional_not,
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, not_vary) {
    scalar::unary orig {
            scalar::unary_operator::conditional_not,
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, not_tv) {
    scalar::unary orig {
            scalar::unary_operator::conditional_not,
            true_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false_or_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, not_fv) {
    scalar::unary orig {
            scalar::unary_operator::conditional_not,
            false_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true_or_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_null_true) {
    scalar::unary orig {
            scalar::unary_operator::is_null,
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_null_false) {
    scalar::unary orig {
            scalar::unary_operator::is_null,
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_null_unknown) {
    scalar::unary orig {
            scalar::unary_operator::is_null,
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_null_vary) {
    scalar::unary orig {
            scalar::unary_operator::is_null,
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_null_tu) {
    scalar::unary orig {
            scalar::unary_operator::is_null,
            true_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_null_fu) {
    scalar::unary orig {
            scalar::unary_operator::is_null,
            false_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_true_true) {
    scalar::unary orig {
            scalar::unary_operator::is_true,
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_true_false) {
    scalar::unary orig {
            scalar::unary_operator::is_true,
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_true_unknown) {
    scalar::unary orig {
            scalar::unary_operator::is_true,
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_true_vary) {
    scalar::unary orig {
            scalar::unary_operator::is_true,
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_true_tu) {
    scalar::unary orig {
            scalar::unary_operator::is_true,
            true_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_true_fu) {
    scalar::unary orig {
            scalar::unary_operator::is_true,
            false_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, is_false_true) {
    scalar::unary orig {
            scalar::unary_operator::is_false,
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_false_false) {
    scalar::unary orig {
            scalar::unary_operator::is_false,
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_false_unknown) {
    scalar::unary orig {
            scalar::unary_operator::is_false,
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_false_vary) {
    scalar::unary orig {
            scalar::unary_operator::is_false,
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_false_tu) {
    scalar::unary orig {
            scalar::unary_operator::is_false,
            true_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, is_false_fu) {
    scalar::unary orig {
            scalar::unary_operator::is_false,
            false_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_unknown_true) {
    scalar::unary orig {
            scalar::unary_operator::is_unknown,
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_unknown_false) {
    scalar::unary orig {
            scalar::unary_operator::is_unknown,
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_unknown_unknown) {
    scalar::unary orig {
            scalar::unary_operator::is_unknown,
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_unknown_vary) {
    scalar::unary orig {
            scalar::unary_operator::is_unknown,
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_unknown_tu) {
    scalar::unary orig {
            scalar::unary_operator::is_unknown,
            true_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, is_unknown_fu) {
    scalar::unary orig {
            scalar::unary_operator::is_unknown,
            false_or_unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig);
}

TEST_F(simplify_predicate_test, and_tt) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(true),
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);
}

TEST_F(simplify_predicate_test, and_tf) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(true),
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, and_tu) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(true),
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);
}

TEST_F(simplify_predicate_test, and_tv) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(true),
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, not_sure());
}

TEST_F(simplify_predicate_test, and_ft) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(false),
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, and_ff) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(false),
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, and_fu) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(false),
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, and_fv) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            boolean(false),
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, and_ut) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            unknown(),
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);
}

TEST_F(simplify_predicate_test, and_uf) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            unknown(),
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, and_uu) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            unknown(),
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);
}

TEST_F(simplify_predicate_test, and_uv) {
    scalar::binary orig {
            scalar::binary_operator::conditional_and,
            unknown(),
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false_or_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig) << *expr;
}

TEST_F(simplify_predicate_test, or_tt) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(true),
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);
}

TEST_F(simplify_predicate_test, or_tf) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(true),
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);
}

TEST_F(simplify_predicate_test, or_tu) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(true),
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);
}

TEST_F(simplify_predicate_test, or_tv) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(true),
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);
}

TEST_F(simplify_predicate_test, or_ft) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(false),
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);
}

TEST_F(simplify_predicate_test, or_ff) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(false),
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_false);
}

TEST_F(simplify_predicate_test, or_fu) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(false),
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);
}

TEST_F(simplify_predicate_test, or_fv) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            boolean(false),
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::not_sure);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, not_sure());
}

TEST_F(simplify_predicate_test, or_ut) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            unknown(),
            boolean(true),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true);
}

TEST_F(simplify_predicate_test, or_uf) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            unknown(),
            boolean(false),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);
}

TEST_F(simplify_predicate_test, or_uu) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            unknown(),
            unknown(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_unknown);
}

TEST_F(simplify_predicate_test, or_uv) {
    scalar::binary orig {
            scalar::binary_operator::conditional_or,
            unknown(),
            not_sure(),
    };
    auto expr = clone_unique<scalar::expression&>(orig);
    auto ref = make_ref(expr);

    auto r = simplify_predicate(ref);
    EXPECT_EQ(r, simplify_predicate_result::constant_true_or_unknown);

    ASSERT_TRUE(expr);
    EXPECT_EQ(*expr, orig) << *expr;
}

} // namespace yugawara::analyzer::details
