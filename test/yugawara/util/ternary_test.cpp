#include <yugawara/util/ternary.h>

#include <gtest/gtest.h>

namespace yugawara::util {

class ternary_test : public ::testing::Test {};

constexpr ternary T = ternary::yes;
constexpr ternary F = ternary::no;
constexpr ternary U = ternary::unknown;

TEST_F(ternary_test, of) {
    EXPECT_EQ(ternary_of(true), T);
    EXPECT_EQ(ternary_of(false), F);
}

TEST_F(ternary_test, eq) {
    EXPECT_EQ(T, true);
    EXPECT_NE(T, false);
    EXPECT_NE(F, true);
    EXPECT_EQ(F, false);
    EXPECT_NE(U, true);
    EXPECT_NE(U, false);
}

TEST_F(ternary_test, not) {
    EXPECT_EQ(~T, F);
    EXPECT_EQ(~U, U);
    EXPECT_EQ(~F, T);
}

TEST_F(ternary_test, or) {
    EXPECT_EQ(T | T, T);
    EXPECT_EQ(T | U, T);
    EXPECT_EQ(T | F, T);

    EXPECT_EQ(U | T, T);
    EXPECT_EQ(U | U, U);
    EXPECT_EQ(U | F, U);

    EXPECT_EQ(F | T, T);
    EXPECT_EQ(F | U, U);
    EXPECT_EQ(F | F, F);
}

TEST_F(ternary_test, and) {
    EXPECT_EQ(T & T, T);
    EXPECT_EQ(T & U, U);
    EXPECT_EQ(T & F, F);

    EXPECT_EQ(U & T, U);
    EXPECT_EQ(U & U, U);
    EXPECT_EQ(U & F, F);

    EXPECT_EQ(F & T, F);
    EXPECT_EQ(F & U, F);
    EXPECT_EQ(F & F, F);
}

TEST_F(ternary_test, output) {
    std::cout << ternary::yes << std::endl;
    std::cout << ternary::no << std::endl;
    std::cout << ternary::unknown << std::endl;
}

} // namespace yugawara::util
