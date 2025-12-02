#include <yugawara/analyzer/details/range_hint.h>

#include <gtest/gtest.h>

#include <stdexcept>

#include <takatori/value/primitive.h>

#include <yugawara/variable/declaration.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

namespace v = ::takatori::value;

// import test utils
using namespace ::yugawara::testing;

class range_hint_test : public ::testing::Test {
protected:
    binding::factory bindings;
};

scalar::immediate const& as_immediate(range_hint_entry::value_type const& value) {
    if (std::holds_alternative<range_hint_entry::immediate_type>(value)) {
        auto&& immediate = std::get<range_hint_entry::immediate_type>(value);
        return *immediate;
    }
    throw std::logic_error("not an immediate value");
}

descriptor::variable const& as_variable(range_hint_entry::value_type const& value) {
    if (std::holds_alternative<range_hint_entry::variable_type>(value)) {
        return std::get<range_hint_entry::variable_type>(value);
    }
    throw std::logic_error("not a variable value");
}

TEST_F(range_hint_test, entry_empty) {
    range_hint_entry entry {};
    EXPECT_TRUE(entry.empty());
    EXPECT_EQ(entry.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(entry.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_intersect_immediate) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(100), true); // 100 <= c
    entry.intersect_upper(constant(200), false); // 200 > c

    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(200));
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(100));
}

TEST_F(range_hint_test, entry_intersect_lower_immediate_smaller) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), true); // 5 <= c

    // intersect with a smaller value
    entry.intersect_lower(constant(4), false); // 4 < c

    // (5 <= c) && (4 < c) -> 5 <= c
    // => keep the original bound
    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(5));
}

TEST_F(range_hint_test, entry_intersect_lower_immediate_larger) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), true); // 5 <= c

    // intersect with a larger value
    entry.intersect_lower(constant(6), false); // 6 < c

    // (5 <= c) && (6 < c) -> 6 < c
    // => intersect with a larger value
    EXPECT_EQ(entry.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(6));
}

TEST_F(range_hint_test, entry_intersect_lower_immediate_equal_inclusive_to_exclusive) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), true); // 5 <= c

    // intersect with an equal value with exclusive bound
    entry.intersect_lower(constant(5), false); // 5 < c

    // (5 <= c) && (5 < c) -> 5 < c
    // => rewrite the inclusiveness
    EXPECT_EQ(entry.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(5));
}

TEST_F(range_hint_test, entry_intersect_lower_immediate_equal_exclusive_to_inclusive) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), false); // 5 < c

    // intersect with an equal value with inclusive bound
    entry.intersect_lower(constant(5), true); // 5 <= c

    // (5 < c) && (5 <= c) -> 5 < c
    // => keep the original bound
    EXPECT_EQ(entry.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(5));
}

TEST_F(range_hint_test, entry_intersect_upper_immediate_smaller) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), true); // 5 >= c

    // intersect with a smaller value
    entry.intersect_upper(constant(4), false); // 4 > c

    // (5 >= c) && (4 > c) -> 4 > c
    // => rewrite the upper bound
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(4));
}

TEST_F(range_hint_test, entry_intersect_upper_immediate_larger) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), true); // 5 >= c

    // intersect with a smaller value
    entry.intersect_upper(constant(6), false); // 6 > c

    // (5 >= c) && (6 > c) -> 5 >= c
    // => keep the original bound
    EXPECT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(5));
}

TEST_F(range_hint_test, entry_intersect_upper_immediate_equal_inclusive_to_exclusive) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), true); // 5 >= c

    // intersect with an equal value with exclusive bound
    entry.intersect_upper(constant(5), false); // 5 > c

    // (5 >= c) && (5 > c) -> 5 > c
    // => rewrite the inclusiveness
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(5));
}

TEST_F(range_hint_test, entry_intersect_upper_immediate_equal_exclusive_to_inclusive) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), false); // 5 > c

    // intersect with an equal value with inclusive bound
    entry.intersect_upper(constant(5), true); // 5 >= c

    // (5 > c) && (5 >= c) -> 5 > c
    // => keep the original bound
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(5));
}

TEST_F(range_hint_test, entry_union_immediate) {
    range_hint_entry entry {};
    entry.union_lower(constant(100), true); // 100 <= c
    entry.union_upper(constant(200), false); // 200 < c

    EXPECT_EQ(entry.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(entry.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_lower_immediate_smaller) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), true); // 5 <= c

    // union with a smaller value
    entry.union_lower(constant(4), false); // 4 < c

    // (5 <= c) || (4 < c) -> 4 < c
    // => rewrite the lower bound
    EXPECT_EQ(entry.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(4));
}

TEST_F(range_hint_test, entry_union_lower_immediate_larger) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), true); // 5 <= c

    // union with a larger value
    entry.union_lower(constant(6), false); // 6 < c

    // (5 <= c) || (6 < c) -> 5 <= c
    // => keep the original bound
    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(5));
}

TEST_F(range_hint_test, entry_union_lower_immediate_equal_inclusive_to_exclusive) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), true); // 5 <= c

    // union with an equal value with exclusive bound
    entry.union_lower(constant(5), false); // 5 < c

    // (5 >= c) || (5 > c) -> 5 >= c
    // => keep the original bound
    ASSERT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(5));
}

TEST_F(range_hint_test, entry_union_lower_immediate_equal_exclusive_to_inclusive) {
    range_hint_entry entry {};
    entry.intersect_lower(constant(5), false); // 5 < c

    // union with an equal value with inclusive bound
    entry.union_lower(constant(5), true); // 5 <= c

    // (5 > c) || (5 >= c) -> 5 >= c
    // => rewrite inclusiveness
    ASSERT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.lower_value()), constant(5));
}

TEST_F(range_hint_test, entry_union_upper_immediate_smaller) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), true); // 5 >= c

    // union with a smaller value
    entry.union_upper(constant(4), false); // 4 > c

    // (5 >= c) || (4 > c) -> 5 >= cc
    // => keep the original bound
    EXPECT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(5));
}

TEST_F(range_hint_test, entry_union_upper_immediate_larger) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), true); // 5 >= c

    // union with a larger value
    entry.union_upper(constant(6), false); // 6 > c

    // (5 >= c) || (6 > c) -> 6 > c
    // => rewrite the upper bound
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(6));
}

TEST_F(range_hint_test, entry_union_upper_immediate_equal_inclusive_to_exclusive) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), true); // 5 >= c

    // union with an equal value with exclusive bound
    entry.union_upper(constant(5), false); // 5 > c

    // (5 >= c) || (5 > c) -> 5 >= c
    // => keep the original bound
    ASSERT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(5));
}

TEST_F(range_hint_test, entry_union_upper_immediate_equal_exclusive_to_inclusive) {
    range_hint_entry entry {};
    entry.intersect_upper(constant(5), false); // 5 > c

    // union with an equal value with inclusive bound
    entry.union_upper(constant(5), true); // 5 >= c

    // (5 > c) || (5 >= c) -> 5 >= c
    // => rewrite inclusiveness
    ASSERT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(entry.upper_value()), constant(5));
}

TEST_F(range_hint_test, entry_intersect_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, true); // v0 <= c
    entry.intersect_upper(v1, false); // v1 > c

    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v1);
}

TEST_F(range_hint_test, entry_intersect_lower_variable_different_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, true); // v0 <= c

    // intersect with a variable
    entry.intersect_lower(v1, false); // v1 < c

    // if conflict variables, keep the first variable
    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_lower_variable_same_variable_inclusive_to_exclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, true); // v0 <= c

    // intersect with a same variable with exclusive bound
    entry.intersect_lower(v0, false); // v0 < c

    // (v0 <= c) && (v0 < c) -> v0 < c
    // => rewrite the inclusiveness
    EXPECT_EQ(entry.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_lower_variable_same_variable_exclusive_to_inclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, false); // v0 < c

    // intersect with a same variable with inclusive bound
    entry.intersect_lower(v0, true); // v0 <= c

    // (v0 < c) && (v0 <= c) -> v0 < c
    // => keep the original bound
    EXPECT_EQ(entry.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_upper_variable_different_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, true); // v0 >= c

    // intersect with a variable
    entry.intersect_upper(v1, false); // v1 > c

    // if conflict variables, keep the first variable
    EXPECT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_upper_variable_same_variable_inclusive_to_exclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, true); // v0 >= c

    // intersect with a same variable with exclusive bound
    entry.intersect_upper(v0, false); // v0 > c

    // (v0 >= c) && (v0 > c) -> v0 > c
    // => rewrite the inclusiveness
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_upper_variable_same_variable_exclusive_to_inclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, false); // v0 > c

    // intersect with a same variable with inclusive bound
    entry.intersect_upper(v0, true); // v0 >= c

    // (v0 > c) && (v0 >= c) -> v0 > c
    // => keep the original bound
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_lower_immediate_not_immediate) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, true); // v0 <= c

    // intersect with an immediate
    entry.intersect_lower(constant(0), false); // 0 < c

    // current implementation, always keep variables in intersect operation
    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_upper_immediate_not_immediate) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, true); // v0 >= c

    // intersect with an immediate
    entry.intersect_upper(constant(0), false); // 0 > c

    // current implementation, always keep variables in intersect operation
    EXPECT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_lower_variable_not_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(constant(0), true); // 0 <= c

    // intersect with a variable
    entry.intersect_lower(v0, false); // v1 < c

    // current implementation, always keep variables in intersect operation
    EXPECT_EQ(entry.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
}

TEST_F(range_hint_test, entry_intersect_upper_variable_not_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(constant(0), true); // 0 >= c

    // intersect with a variable
    entry.intersect_upper(v0, false); // v1 > c

    // current implementation, always keep variables in intersect operation
    EXPECT_EQ(entry.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v0);
}

TEST_F(range_hint_test, entry_union_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry entry {};
    entry.union_lower(v0, true); // v0 <= c
    entry.union_upper(v1, false); // v1 > c

    EXPECT_EQ(entry.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(entry.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_lower_variable_different_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, true); // v0 <= c

    // union with a different variable
    entry.union_lower(v1, false); // v1 < c

    // (v0 <= c) || (v1 < c) -> unknown, as different variables are not comparable
    EXPECT_EQ(entry.lower_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_lower_variable_same_variable_inclusive_to_exclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, true); // v0 <= c

    // union with a same variable with exclusive bound
    entry.union_lower(v0, false); // v0 < c

    // (v0 <= c) || (v0 < c) -> v0 <= c
    // => keep the original bound
    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
}

TEST_F(range_hint_test, entry_union_lower_variable_same_variable_exclusive_to_inclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, false); // v0 < c

    // union with a same variable with inclusive bound
    entry.union_lower(v0, true); // v0 <= c

    // (v0 < c) || (v0 <= c) -> v0 <= c
    // => rewrite inclusiveness
    EXPECT_EQ(entry.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.lower_value()), v0);
}

TEST_F(range_hint_test, entry_union_upper_variable_different_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, true); // v0 >= c

    // union with a variable
    entry.union_upper(v1, false); // v1 > c

    // (v0 >= c) || (v1 > c) -> unknown, as different variables are not comparable
    EXPECT_EQ(entry.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_upper_variable_same_variable_inclusive_to_exclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, true); // v0 >= c

    // union with a same variable with exclusive bound
    entry.union_upper(v0, false); // v0 > c

    // (v0 >= c) || (v0 > c) -> v0 >= c
    // => keep the original bound
    EXPECT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v0);
}

TEST_F(range_hint_test, entry_union_upper_variable_same_variable_exclusive_to_inclusive) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, false); // v0 > c

    // union with a same variable with inclusive bound
    entry.union_upper(v0, true); // v0 >= c

    // (v0 > c) || (v0 >= c) -> v0 >= c
    // => rewrite inclusiveness
    EXPECT_EQ(entry.upper_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(entry.upper_value()), v0);
}

TEST_F(range_hint_test, entry_union_lower_immediate_not_immediate) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(v0, true); // v0 <= c

    // union with an immediate
    entry.union_lower(constant(0), false); // 0 < c

    // (v0 <= c) || (0 < c) -> unknown, as different types are not comparable
    EXPECT_EQ(entry.lower_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_upper_immediate_not_immediate) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(v0, true); // v0 >= c

    // union with an immediate
    entry.union_upper(constant(0), false); // 0 > c

    // (v0 >= c) || (0 > c) -> unknown, as different types are not comparable
    EXPECT_EQ(entry.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_lower_variable_not_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_lower(constant(0), true); // 0 <= c

    // union with a variable
    entry.union_lower(v0, false); // v0 < c

    // (0 <= c) || (v0 < c) -> unknown, as different types are not comparable
    EXPECT_EQ(entry.lower_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_upper_variable_not_variable) {
    auto v0 = bindings.external_variable({ "v0", t::int4 {} });

    range_hint_entry entry {};
    entry.intersect_upper(constant(0), true); // 0 >= c

    // union with a variable
    entry.union_upper(v0, false); // v0 > c

    // (0 >= c) || (v0 > c) -> unknown, as different types are not comparable
    EXPECT_EQ(entry.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_intersect_merge_infinity) {
    range_hint_entry left {};
    left.intersect_lower(constant(100), true); // 100 <= c
    left.intersect_upper(constant(200), false); // 200 > c

    range_hint_entry right {};

    // (100 <= c < 200) && ( -infinity < c < +infinity ) -> (100 <= c < 200)
    left.intersect_merge(std::move(right));
    EXPECT_EQ(left.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(left.lower_value()), constant(100));
    EXPECT_EQ(left.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(left.upper_value()), constant(200));
}

TEST_F(range_hint_test, entry_intersect_merge_immediate) {
    range_hint_entry left {};
    left.intersect_lower(constant(100), true); // 100 <= c
    left.intersect_upper(constant(200), false); // 200 > c

    range_hint_entry right {};
    right.intersect_lower(constant(110), true); // 110 <= c
    right.intersect_upper(constant(190), false); // 190 > c

    // (100 <= c < 200) && (110 <= c < 190) -> (110 <= c < 190)
    left.intersect_merge(std::move(right));
    EXPECT_EQ(left.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(left.lower_value()), constant(110));
    EXPECT_EQ(left.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(left.upper_value()), constant(190));
}

TEST_F(range_hint_test, entry_intersect_merge_variable) {
    range_hint_entry left {};
    left.intersect_lower(constant(100), true); // 100 <= c
    left.intersect_upper(constant(200), false); // 200 > c

    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry right {};
    right.intersect_lower(v0, true); // v0 <= c
    right.intersect_upper(v1, false); // v1 > c

    left.intersect_merge(std::move(right));

    // (100 <= c < 200) && (v0 <= c < v1) -> (v0 <= c < v1)
    EXPECT_EQ(left.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_variable(left.lower_value()), v0);
    EXPECT_EQ(left.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_variable(left.upper_value()), v1);
}

TEST_F(range_hint_test, entry_union_merge_infinity) {
    range_hint_entry left {};
    left.intersect_lower(constant(110), true); // 110 < c
    left.intersect_upper(constant(190), false); // 190 >= c

    range_hint_entry right {};

    // (110 < c < 190) || (-Inf < c < +Inf) -> (-Inf < c < +Inf)
    left.union_merge(std::move(right));
    EXPECT_EQ(left.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(left.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, entry_union_merge_immediate) {
    range_hint_entry left {};
    left.intersect_lower(constant(110), true); // 110 <= c
    left.intersect_upper(constant(190), false); // 190 > c

    range_hint_entry right {};
    right.intersect_lower(constant(100), true); // 100 <= c
    right.intersect_upper(constant(200), false); // 200 > c

    // (110 <= c < 190) || (100 <= c < 200) -> (100 <= c < 200)
    left.union_merge(std::move(right));
    EXPECT_EQ(left.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(left.lower_value()), constant(100));
    EXPECT_EQ(left.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(left.upper_value()), constant(200));
}

TEST_F(range_hint_test, entry_union_merge_variable) {
    range_hint_entry left {};
    left.intersect_lower(constant(100), true); // 100 <= c
    left.intersect_upper(constant(200), false); // 200 > c

    auto v0 = bindings.external_variable({ "v0", t::int4 {} });
    auto v1 = bindings.external_variable({ "v1", t::int4 {} });

    range_hint_entry right {};
    right.intersect_lower(v0, true); // v0 <= c
    right.intersect_upper(v1, false); // v1 > c

    // (100 <= c < 200) || (v0 <= c < v1) -> (-Inf < c < +Inf)
    left.union_merge(std::move(right));
    EXPECT_EQ(left.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(left.upper_type(), range_hint_type::infinity);
}

TEST_F(range_hint_test, map_simple) {
    range_hint_map map {};

    auto c1 = bindings.stream_variable("c1");
    EXPECT_FALSE(map.contains(c1));

    auto& entry = map.get(c1);
    EXPECT_FALSE(map.contains(c1));

    entry.intersect_lower(constant(0), true); // 0 <= c1
    EXPECT_TRUE(map.contains(c1));

    bool saw = false;
    map.consume([&](range_hint_map::key_type const& key, range_hint_map::value_type&& value) -> void {
        if (saw) {
            throw std::logic_error("already saw an entry");
        }
        EXPECT_EQ(key, c1);
        EXPECT_EQ(value.lower_type(), range_hint_type::inclusive);
        EXPECT_EQ(as_immediate(value.lower_value()), constant(0));
        saw = true;
    });

    EXPECT_TRUE(saw);
}

TEST_F(range_hint_test, map_intersect_merge) {
    range_hint_map m1 {};
    range_hint_map m2 {};

    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");

    // m1:
    //   c1: (-Inf, +Inf)
    //   c2: (0, +Inf)
    //   c3: (-Inf, 100)
    auto&& m1c2 = m1.get(c2);
    m1c2.intersect_lower(constant(0), false); // 0 < c2
    auto&& m1c3 = m1.get(c3);
    m1c3.intersect_upper(constant(100), false); // 100 > c3

    // m2:
    //   c1: (0, +Inf)
    //   c2: (-Inf, 50)
    //   c3: (-Inf, +Inf)
    auto && m2c1 = m2.get(c1);
    m2c1.intersect_lower(constant(0), false); // 0 < c1
    auto && m2c2 = m2.get(c2);
    m2c2.intersect_upper(constant(50), false); // 50 > c2

    m1.intersect_merge(std::move(m2));

    // after merge, m1:
    //   c1: (0, +Inf)
    //   c2: (0, 50)
    //   c3: (-Inf, 100)
    auto&& e1 = m1.get(c1);
    EXPECT_EQ(e1.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(e1.lower_value()), constant(0));
    EXPECT_EQ(e1.upper_type(), range_hint_type::infinity);

    auto&& e2 = m1.get(c2);
    EXPECT_EQ(e2.lower_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(e2.lower_value()), constant(0));
    EXPECT_EQ(e2.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(e2.upper_value()), constant(50));

    auto&& e3 = m1.get(c3);
    EXPECT_EQ(e3.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(e3.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(e3.upper_value()), constant(100));
}

TEST_F(range_hint_test, map_union_merge) {
    range_hint_map m1 {};
    range_hint_map m2 {};

    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto c4 = bindings.stream_variable("c4");

    // m1:
    //   c1: [0, 100)
    //   c2: (0, Inf)
    //   c3: (0, Inf)
    //   c4: (Inf, Inf)
    auto&& m1c1 = m1.get(c1);
    m1c1.intersect_lower(constant(0), true); // 0 <= c1
    m1c1.intersect_upper(constant(100), false); // 100 > c1
    auto&& m1c2 = m1.get(c2);
    m1c2.intersect_lower(constant(0), false); // 0 < c2
    auto&& m1c3 = m1.get(c3);
    m1c3.intersect_lower(constant(0), false); // 0 < c3

    // m2:
    //   c1: (0, 200)
    //   c2: (Inf, 50)
    //   c3: (Inf, Inf)
    //   c4: (0, Inf)
    auto && m2c1 = m2.get(c1);
    m2c1.intersect_lower(constant(0), false); // 0 < c1
    m2c1.intersect_upper(constant(200), false); // 200 > c1
    auto && m2c2 = m2.get(c2);
    m2c2.intersect_upper(constant(50), false); // 50 > c2
    auto && m2c4 = m2.get(c4);
    m2c4.intersect_lower(constant(0), false); // 0 < c4

    m1.union_merge(std::move(m2));

    // after merge, m1:
    //   c1: [0, 200)
    //   c2: (Inf, Inf)
    //   c3: (Inf, Inf)
    //   c4: (Inf, Inf)
    auto&& e1 = m1.get(c1);
    EXPECT_EQ(e1.lower_type(), range_hint_type::inclusive);
    EXPECT_EQ(as_immediate(e1.lower_value()), constant(0));
    EXPECT_EQ(e1.upper_type(), range_hint_type::exclusive);
    EXPECT_EQ(as_immediate(e1.upper_value()), constant(200));

    auto&& e2 = m1.get(c2);
    EXPECT_EQ(e2.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(e2.upper_type(), range_hint_type::infinity);

    auto&& e3 = m1.get(c3);
    EXPECT_EQ(e3.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(e3.upper_type(), range_hint_type::infinity);

    auto&& e4 = m1.get(c4);
    EXPECT_EQ(e4.lower_type(), range_hint_type::infinity);
    EXPECT_EQ(e4.upper_type(), range_hint_type::infinity);
}

} // namespace yugawara::analyzer::details
