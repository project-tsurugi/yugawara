#include <yugawara/storage/column_value.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>

#include <yugawara/function/declaration.h>

namespace yugawara::storage {

class column_value_test : public ::testing::Test {};

namespace v = ::takatori::value;

TEST_F(column_value_test, nothing) {
    column_value c {};

    EXPECT_EQ(c.kind(), column_value_kind::nothing);
}

TEST_F(column_value_test, nullptr) {
    column_value c { nullptr };

    EXPECT_EQ(c.kind(), column_value_kind::nothing);
}

TEST_F(column_value_test, immediate) {
    column_value c { v::int4(100) };

    ASSERT_EQ(c.kind(), column_value_kind::immediate);
    EXPECT_EQ(*c.element<column_value_kind::immediate>(), v::int4(100));
}

TEST_F(column_value_test, function) {
    auto f = std::make_shared<function::declaration>(function::declaration {
            1,
            "f",
            ::takatori::type::int4 {},
            {},
    });
    column_value c { f };

    ASSERT_EQ(c.kind(), column_value_kind::function);
    EXPECT_EQ(*c.element<column_value_kind::function>(), *f);
}

TEST_F(column_value_test, sequence) {
    auto s = std::make_shared<sequence>("S1");
    column_value c { s };

    ASSERT_EQ(c.kind(), column_value_kind::sequence);
    EXPECT_EQ(*c.element<column_value_kind::sequence>(), *s);
}

TEST_F(column_value_test, eq_immediate) {
    column_value a { v::int4(1) };
    column_value b { v::int4(2) };
    column_value c { v::int4(1) };

    EXPECT_EQ(a, a);
    EXPECT_NE(a, b);
    EXPECT_EQ(a, c);
}

TEST_F(column_value_test, eq_function) {
    auto f1 = std::make_shared<function::declaration>(function::declaration {
            1,
            "f1",
            ::takatori::type::int4 {},
            {},
    });
    auto f2 = std::make_shared<function::declaration>(function::declaration {
            2,
            "f2",
            ::takatori::type::int4 {},
            {},
    });
    column_value a { f1 };
    column_value b { f2 };
    column_value c { f1 };

    EXPECT_EQ(a, a);
    EXPECT_NE(a, b);
    EXPECT_EQ(a, c);
}

TEST_F(column_value_test, eq_sequence) {
    auto s1 = std::make_shared<sequence>("S1");
    auto s2 = std::make_shared<sequence>("S1");
    column_value a { s1 };
    column_value b { s2 };
    column_value c { s1 };

    EXPECT_EQ(a, a);
    EXPECT_NE(a, b);
    EXPECT_EQ(a, c);
}

TEST_F(column_value_test, output) {
    column_value c { v::int4(100) };
    std::cout << c << std::endl;
}

} // namespace yugawara::storage
