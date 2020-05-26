#include <yugawara/analyzer/details/decompose_predicate.h>

#include <gtest/gtest.h>

#include <takatori/scalar/binary.h>
#include <takatori/scalar/unary.h>
#include <takatori/util/rvalue_reference_wrapper.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::scalar::unary;
using ::takatori::scalar::binary;
using ::takatori::scalar::unary_operator;
using ::takatori::scalar::binary_operator;

using ::takatori::util::object_ownership_reference;
using ::takatori::util::rvalue_reference_wrapper;

using details::decompose_predicate;

class predicate_decomposer_test : public ::testing::Test {
protected:
    std::vector<object_ownership_reference<scalar::expression>> results;
    details::predicate_consumer consumer = [this](object_ownership_reference<scalar::expression>&& expr) {
        results.emplace_back(std::move(expr));
    };

    bool eq(std::initializer_list<rvalue_reference_wrapper<scalar::expression>> as) {
        if (as.size() != results.size()) {
            return false;
        }
        // NOTE: no duplicates in as
        for (auto&& a : as) {
            bool found = false;
            for (auto it = results.begin(); it != results.end(); ++it) {
                if (a.get() == **it) {
                    found = true;
                    results.erase(it);
                    break;
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }
};

TEST_F(predicate_decomposer_test, simple) {
    auto expr = constant(100);
    decompose_predicate(expr, consumer);

    EXPECT_TRUE(eq({}));
}

TEST_F(predicate_decomposer_test, and) {
    binary expr {
            binary_operator::conditional_and,
            constant(1),
            constant(2),
    };

    decompose_predicate(expr, consumer);

    EXPECT_TRUE(eq({
            constant(1),
            constant(2),
    }));
}

TEST_F(predicate_decomposer_test, or) {
    binary expr {
            binary_operator::conditional_or,
            constant(1),
            constant(2),
    };

    decompose_predicate(expr, consumer);

    EXPECT_TRUE(eq({}));
}

TEST_F(predicate_decomposer_test, nested) {
    binary expr {
            binary_operator::conditional_and,
            binary {
                binary_operator::conditional_and,
                        constant(1),
                        constant(2),
            },
            binary {
                    binary_operator::conditional_and,
                    constant(3),
                    constant(4),
            },
    };

    decompose_predicate(expr, consumer);

    EXPECT_EQ(results.size(), 4);
    EXPECT_TRUE(eq({
            constant(1),
            constant(2),
            constant(3),
            constant(4),
    }));
}

TEST_F(predicate_decomposer_test, self) {
    unary expr {
            unary_operator::conditional_not,
            constant(1),
    };

    decompose_predicate(expr.ownership_operand(), consumer);

    EXPECT_TRUE(eq({
            constant(1),
    }));
}

} // namespace yugawara::analyzer::details
