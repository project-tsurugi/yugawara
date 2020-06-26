#include <yugawara/extension/scalar/aggregate_function_call.h>

#include <type_traits>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>
#include <takatori/scalar/immediate.h>

#include <takatori/util/clonable.h>

#include <yugawara/binding/factory.h>

namespace yugawara::extension::scalar {

class aggregate_function_call_test : public ::testing::Test {
public:
    binding::factory bindings;
    ::takatori::descriptor::aggregate_function desc = bindings(aggregate::declaration {
            aggregate::declaration::minimum_builtin_function_id + 1,
            "testing",
            ::takatori::type::int4 {},
            {
                    ::takatori::type::int4 {},
            },
            true,
    });
};

static_assert(aggregate_function_call::tag == ::takatori::scalar::extension::tag);
static_assert(aggregate_function_call::extension_tag == extension_id::aggregate_function_call_id);

using ::takatori::util::clone_unique;

static ::takatori::scalar::immediate constant(int v) {
    return ::takatori::scalar::immediate {
            ::takatori::value::int4 { v },
            ::takatori::type::int4  {},
    };
}

TEST_F(aggregate_function_call_test, simple) {
    aggregate_function_call expr { desc };
    EXPECT_EQ(expr.function(), desc);

    ASSERT_EQ(expr.arguments().size(), 0);
}

TEST_F(aggregate_function_call_test, arguments) {
    aggregate_function_call expr {
            desc,
            {
                    constant(100),
            },
    };
    EXPECT_EQ(expr.function(), desc);

    ASSERT_EQ(expr.arguments().size(), 1);
    ASSERT_EQ(expr.arguments()[0], constant(100));
}

TEST_F(aggregate_function_call_test, arguments_multiple) {
    aggregate_function_call expr {
            desc,
            {
                    constant(100),
                    constant(200),
                    constant(300),
            },
    };
    EXPECT_EQ(expr.function(), desc);

    ASSERT_EQ(expr.arguments().size(), 3);
    ASSERT_EQ(expr.arguments()[0], constant(100));
    ASSERT_EQ(expr.arguments()[1], constant(200));
    ASSERT_EQ(expr.arguments()[2], constant(300));
}

TEST_F(aggregate_function_call_test, clone) {
    aggregate_function_call expr {
            desc,
            {
                    constant(100),
                    constant(200),
                    constant(300),
            },
    };

    auto copy = clone_unique(expr);
    EXPECT_EQ(expr, *copy);
    EXPECT_NE(std::addressof(expr), copy.get());
}

TEST_F(aggregate_function_call_test, output) {
    aggregate_function_call expr {
            desc,
            {
                    constant(100),
                    constant(200),
                    constant(300),
            },
    };
    std::cout << expr << std::endl;
}

} // namespace takatori::scalar
