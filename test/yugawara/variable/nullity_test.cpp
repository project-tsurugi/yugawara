#include <yugawara/variable/nullity.h>

#include <gtest/gtest.h>

namespace yugawara::variable {

class nullity_test : public ::testing::Test {};

TEST_F(nullity_test, not) {
    EXPECT_EQ(~nullable, nullity(false));
    EXPECT_EQ(~~nullable, nullable);
}

TEST_F(nullity_test, and) {
    EXPECT_EQ( nullable &  nullable,  nullable);
    EXPECT_EQ( nullable & ~nullable, ~nullable);
    EXPECT_EQ(~nullable &  nullable, ~nullable);
    EXPECT_EQ(~nullable & ~nullable, ~nullable);
}

TEST_F(nullity_test, or) {
    EXPECT_EQ( nullable |  nullable,  nullable);
    EXPECT_EQ( nullable | ~nullable, nullable);
    EXPECT_EQ(~nullable |  nullable, nullable);
    EXPECT_EQ(~nullable | ~nullable, ~nullable);
}

TEST_F(nullity_test, output) {
    std::cout << nullable << std::endl;
    std::cout << ~nullable << std::endl;
}

} // namespace yugawara::variable
