#include "yugawara/util/either.h"

#include <gtest/gtest.h>

namespace yugawara::util {

class either_test : public ::testing::Test {
public:
    template<class T>
    static T const& make_const(T const& v) { return v; }
};

TEST_F(either_test, empty) {
    either<bool, int> e;

    ASSERT_FALSE(e);
    EXPECT_FALSE(e.has_value());
    EXPECT_TRUE(e.is_error());
    EXPECT_EQ(e.error(), false);
}

TEST_F(either_test, normal) {
    either<bool, int> e { make_const(100) };

    ASSERT_TRUE(e);
    EXPECT_TRUE(e.has_value());
    EXPECT_FALSE(e.is_error());
    EXPECT_EQ(e.value(), 100);
}

TEST_F(either_test, normal_rvalue) {
    either<bool, int> e { 100 };

    ASSERT_TRUE(e);
    EXPECT_EQ(e.value(), 100);
}

TEST_F(either_test, assign_normal) {
    either<bool, int> e;
    e = make_const(100);

    ASSERT_TRUE(e);
    EXPECT_EQ(e.value(), 100);
}

TEST_F(either_test, assign_normal_rvalue) {
    either<bool, int> e;
    e = 100;

    ASSERT_TRUE(e);
    EXPECT_EQ(e.value(), 100);
}

TEST_F(either_test, error) {
    either<bool, int> e { make_const(true) };

    ASSERT_FALSE(e);
    EXPECT_FALSE(e.has_value());
    EXPECT_TRUE(e.is_error());
    EXPECT_EQ(e.error(), true);
}

TEST_F(either_test, error_rvalue) {
    either<bool, int> e { true };

    ASSERT_FALSE(e);
    EXPECT_EQ(e.error(), true);
}

TEST_F(either_test, assign_error) {
    either<bool, int> e;
    e = make_const(true);

    ASSERT_FALSE(e);
    EXPECT_EQ(e.error(), true);
}

TEST_F(either_test, assign_error_rvalue) {
    either<bool, int> e;
    e = true;

    ASSERT_FALSE(e);
    EXPECT_EQ(e.error(), true);
}

TEST_F(either_test, inplace_normal) {
    either<bool, int> e { std::in_place_type<int>, 100 };

    ASSERT_TRUE(e);
    EXPECT_EQ(e.value(), 100);
}

TEST_F(either_test, inplace_error) {
    either<bool, int> e { std::in_place_type<bool>, true };

    ASSERT_FALSE(e);
    EXPECT_EQ(e.error(), true);
}

TEST_F(either_test, emplace_normal) {
    either<bool, int> e;
    e.emplace<int>(100);

    ASSERT_TRUE(e);
    EXPECT_EQ(e.value(), 100);
}

TEST_F(either_test, emplace_error) {
    either<bool, int> e;
    e.emplace<bool>(false);

    ASSERT_FALSE(e);
    EXPECT_EQ(e.error(), false);
}

TEST_F(either_test, value_normal) {
    either<bool, int> e { 100 };
    EXPECT_EQ(e.value(), 100);
}

TEST_F(either_test, value_error) {
    either<bool, int> e { true };
    EXPECT_ANY_THROW(e.value());
}

TEST_F(either_test, get_normal) {
    either<bool, int> e { 100 };
    auto* p = e.get();
    ASSERT_TRUE(p);
    EXPECT_EQ(*p, 100);
}

TEST_F(either_test, get_error) {
    either<bool, int> e { true };
    auto* p = e.get();
    EXPECT_FALSE(p);
}

TEST_F(either_test, error_normal) {
    either<bool, int> e { 100 };
    EXPECT_ANY_THROW(e.error());
}

TEST_F(either_test, error_error) {
    either<bool, int> e { true };
    EXPECT_EQ(e.error(), true);
}

TEST_F(either_test, compare) {
    using E = either<bool, int>;

    EXPECT_EQ(E(1), E(1));
    EXPECT_NE(E(1), E(2));

    EXPECT_EQ(E(true), E(true));
    EXPECT_NE(E(true), E(false));

    EXPECT_NE(E(0), E(false));
    EXPECT_NE(E(false), E(0));
}

TEST_F(either_test, output_normal) {
    either<bool, int> e { 100 };
    std::cout << e << std::endl;
}

TEST_F(either_test, output_error) {
    either<bool, int> e { true };
    std::cout << e << std::endl;
}

} // namespace yugawara::util
