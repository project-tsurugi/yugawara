#include <yugawara/util/move_only.h>

#include <optional>
#include <type_traits>

#include <gtest/gtest.h>

namespace yugawara::util {

class move_only_test : public ::testing::Test {};

static_assert(std::is_default_constructible_v<move_only<int>>);
static_assert(std::is_constructible_v<move_only<int>, int>);
static_assert(!std::is_copy_constructible_v<move_only<int>>);
static_assert(!std::is_copy_assignable_v<move_only<int>>);
static_assert(std::is_move_constructible_v<move_only<int>>);
static_assert(std::is_move_assignable_v<move_only<int>>);

TEST_F(move_only_test, simple) {
    move_only<int> v { 100 };

    EXPECT_EQ(*v, 100);

    auto x = std::move(v);
    EXPECT_EQ(*x, 100);
}

TEST_F(move_only_test, optional) {
    move_only<std::optional<int>> v { 100 };

    EXPECT_EQ(*v, 100);

    auto x = std::move(v);
    EXPECT_EQ(*x, 100);
}

} // namespace yugawara::util
