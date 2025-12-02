#include <yugawara/analyzer/details/compare_value.h>

#include <gtest/gtest.h>

#include <limits>

#include <takatori/value/primitive.h>
#include <takatori/value/decimal.h>
#include <takatori/value/character.h>
#include <takatori/value/octet.h>
#include <takatori/value/bit.h>
#include <takatori/value/date.h>
#include <takatori/value/time_of_day.h>
#include <takatori/value/time_point.h>


namespace yugawara::analyzer::details {

class compare_value_test : public ::testing::Test {};

namespace v = ::takatori::value;

TEST_F(compare_value_test, boolean) {
    // boolean
    EXPECT_EQ(compare(v::boolean { true }, v::boolean { true }), compare_result::equal);
    EXPECT_EQ(compare(v::boolean { false }, v::boolean { true }), compare_result::less);
    EXPECT_EQ(compare(v::boolean { true }, v::boolean { false }), compare_result::greater);
    EXPECT_EQ(compare(v::boolean { false }, v::boolean { false }), compare_result::equal);

    v::boolean value { true };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "c" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, int4) {
    // int4
    EXPECT_EQ(compare(v::int4 { 10 }, v::int4 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::int4 { 10 }, v::int4 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::int4 { 10 }, v::int4 {  5 }), compare_result::greater);

    // int8
    EXPECT_EQ(compare(v::int4 { 10 }, v::int8 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::int4 { 10 }, v::int8 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::int4 { 10 }, v::int8 {  5 }), compare_result::greater);

    // decimal
    EXPECT_EQ(compare(v::int4 { 10 }, v::decimal { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::int4 { 10 }, v::decimal { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::int4 { 10 }, v::decimal {  5 }), compare_result::greater);

    v::int4 value { 0 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "c" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, int8) {
    // int4
    EXPECT_EQ(compare(v::int8 { 10 }, v::int4 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::int8 { 10 }, v::int4 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::int8 { 10 }, v::int4 {  5 }), compare_result::greater);

    // int8
    EXPECT_EQ(compare(v::int8 { 10 }, v::int8 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::int8 { 10 }, v::int8 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::int8 { 10 }, v::int8 {  5 }), compare_result::greater);

    // decimal
    EXPECT_EQ(compare(v::int8 { 10 }, v::decimal { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::int8 { 10 }, v::decimal { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::int8 { 10 }, v::decimal {  5 }), compare_result::greater);

    v::int4 value { 0 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "c" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, decimal) {
    // int4
    EXPECT_EQ(compare(v::decimal { 10 }, v::int4 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::decimal { 10 }, v::int4 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::decimal { 10 }, v::int4 {  5 }), compare_result::greater);

    // int8
    EXPECT_EQ(compare(v::decimal { 10 }, v::int8 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::decimal { 10 }, v::int8 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::decimal { 10 }, v::int8 {  5 }), compare_result::greater);

    // decimal
    EXPECT_EQ(compare(v::decimal { 10 }, v::decimal { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::decimal { 10 }, v::decimal { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::decimal { 10 }, v::decimal {  5 }), compare_result::greater);

    v::int4 value { 0 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "c" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, float4) {
    // float4
    EXPECT_EQ(compare(v::float4 { 10 }, v::float4 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float4 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float4 {  5 }), compare_result::greater);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float4 { std::numeric_limits<float>::quiet_NaN() }), compare_result::undefined);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float4 { +std::numeric_limits<float>::infinity() }), compare_result::less);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float4 { -std::numeric_limits<float>::infinity() }), compare_result::greater);

    // float8
    EXPECT_EQ(compare(v::float4 { 10 }, v::float8 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float8 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float8 {  5 }), compare_result::greater);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float8 { std::numeric_limits<float>::quiet_NaN() }), compare_result::undefined);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float8 { +std::numeric_limits<float>::infinity() }), compare_result::less);
    EXPECT_EQ(compare(v::float4 { 10 }, v::float8 { -std::numeric_limits<float>::infinity() }), compare_result::greater);

    // abnormal values
    EXPECT_EQ(compare(
        v::float4 { std::numeric_limits<float>::quiet_NaN() },
        v::float4 { 0 }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float4 { std::numeric_limits<float>::quiet_NaN() },
        v::float4 { +std::numeric_limits<float>::infinity() }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float4 { std::numeric_limits<float>::quiet_NaN() },
        v::float4 { -std::numeric_limits<float>::infinity() }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float4 { +std::numeric_limits<float>::infinity() },
        v::float4 { 0 }), compare_result::greater);
    EXPECT_EQ(compare(
        v::float4 { +std::numeric_limits<float>::infinity() },
        v::float4 { +std::numeric_limits<float>::infinity() }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float4 { +std::numeric_limits<float>::infinity() },
        v::float4 { -std::numeric_limits<float>::infinity() }), compare_result::greater);
    EXPECT_EQ(compare(
        v::float4 { -std::numeric_limits<float>::infinity() },
        v::float4 { 0 }), compare_result::less);
    EXPECT_EQ(compare(
        v::float4 { -std::numeric_limits<float>::infinity() },
        v::float4 { +std::numeric_limits<float>::infinity() }), compare_result::less);
    EXPECT_EQ(compare(
        v::float4 { -std::numeric_limits<float>::infinity() },
        v::float4 { -std::numeric_limits<float>::infinity() }), compare_result::undefined);

    v::float4 value { 0 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "c" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, float8) {
    // float4
    EXPECT_EQ(compare(v::float8 { 10 }, v::float4 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float4 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float4 {  5 }), compare_result::greater);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float4 { std::numeric_limits<float>::quiet_NaN() }), compare_result::undefined);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float4 { +std::numeric_limits<float>::infinity() }), compare_result::less);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float4 { -std::numeric_limits<float>::infinity() }), compare_result::greater);

    // float8
    EXPECT_EQ(compare(v::float8 { 10 }, v::float8 { 10 }), compare_result::equal);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float8 { 15 }), compare_result::less);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float8 {  5 }), compare_result::greater);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float8 { std::numeric_limits<float>::quiet_NaN() }), compare_result::undefined);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float8 { +std::numeric_limits<float>::infinity() }), compare_result::less);
    EXPECT_EQ(compare(v::float8 { 10 }, v::float8 { -std::numeric_limits<float>::infinity() }), compare_result::greater);

    // abnormal values
    EXPECT_EQ(compare(
        v::float8 { std::numeric_limits<float>::quiet_NaN() },
        v::float8 { 0 }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float8 { std::numeric_limits<float>::quiet_NaN() },
        v::float8 { +std::numeric_limits<float>::infinity() }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float8 { std::numeric_limits<float>::quiet_NaN() },
        v::float8 { -std::numeric_limits<float>::infinity() }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float8 { +std::numeric_limits<float>::infinity() },
        v::float8 { 0 }), compare_result::greater);
    EXPECT_EQ(compare(
        v::float8 { +std::numeric_limits<float>::infinity() },
        v::float8 { +std::numeric_limits<float>::infinity() }), compare_result::undefined);
    EXPECT_EQ(compare(
        v::float8 { +std::numeric_limits<float>::infinity() },
        v::float8 { -std::numeric_limits<float>::infinity() }), compare_result::greater);
    EXPECT_EQ(compare(
        v::float8 { -std::numeric_limits<float>::infinity() },
        v::float8 { 0 }), compare_result::less);
    EXPECT_EQ(compare(
        v::float8 { -std::numeric_limits<float>::infinity() },
        v::float8 { +std::numeric_limits<float>::infinity() }), compare_result::less);
    EXPECT_EQ(compare(
        v::float8 { -std::numeric_limits<float>::infinity() },
        v::float8 { -std::numeric_limits<float>::infinity() }), compare_result::undefined);

    v::float8 value { 0 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "c" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, character) {
    // character
    EXPECT_EQ(compare(v::character { "CC" }, v::character { "CC" }), compare_result::equal);
    EXPECT_EQ(compare(v::character { "CC" }, v::character { "B" }), compare_result::greater);
    EXPECT_EQ(compare(v::character { "CC" }, v::character { "D" }), compare_result::less);
    EXPECT_EQ(compare(v::character { "CC" }, v::character { "CB" }), compare_result::greater);
    EXPECT_EQ(compare(v::character { "CC" }, v::character { "CD" }), compare_result::less);
    EXPECT_EQ(compare(v::character { "CC" }, v::character { "CC0" }), compare_result::less);

    v::character value { "c" };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, octet) {
    // character
    EXPECT_EQ(compare(v::octet { "CC" }, v::octet { "CC" }), compare_result::equal);
    EXPECT_EQ(compare(v::octet { "CC" }, v::octet { "B" }), compare_result::greater);
    EXPECT_EQ(compare(v::octet { "CC" }, v::octet { "D" }), compare_result::less);
    EXPECT_EQ(compare(v::octet { "CC" }, v::octet { "CB" }), compare_result::greater);
    EXPECT_EQ(compare(v::octet { "CC" }, v::octet { "CD" }), compare_result::less);
    EXPECT_EQ(compare(v::octet { "CC" }, v::octet { "CC0" }), compare_result::less);

    v::octet value { "c" };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, date) {
    // date
    EXPECT_EQ(compare(v::date { 2001, 1, 1 }, v::date { 2001,  1,  1 }), compare_result::equal);
    EXPECT_EQ(compare(v::date { 2001, 1, 1 }, v::date { 2000, 12, 31 }), compare_result::greater);
    EXPECT_EQ(compare(v::date { 2001, 1, 1 }, v::date { 2001,  1,  2 }), compare_result::less);

    v::date value { 1970, 1, 1 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, time_of_day) {
    // time_of_day
    EXPECT_EQ(compare(v::time_of_day { 12, 0, 0 }, v::time_of_day { 12,  0,  0 }), compare_result::equal);
    EXPECT_EQ(compare(v::time_of_day { 12, 0, 0 }, v::time_of_day { 11, 59, 59 }), compare_result::greater);
    EXPECT_EQ(compare(v::time_of_day { 12, 0, 0 }, v::time_of_day { 12,  0,  1 }), compare_result::less);
    EXPECT_EQ(compare(
            v::time_of_day { 12, 0, 0, std::chrono::milliseconds(100) },
            v::time_of_day { 12, 0, 0, std::chrono::milliseconds(100) }), compare_result::equal);
    EXPECT_EQ(compare(
            v::time_of_day { 12, 0, 0, std::chrono::milliseconds(100) },
            v::time_of_day { 12, 0, 0, std::chrono::milliseconds(101) }), compare_result::less);
    EXPECT_EQ(compare(
            v::time_of_day { 12, 0, 0, std::chrono::milliseconds(100) },
            v::time_of_day { 12, 0, 0, std::chrono::milliseconds( 99) }), compare_result::greater);

    v::time_of_day value { 0, 0, 0 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, time_point) {
    // time_of_day
    EXPECT_EQ(compare(
        v::time_point { 2001, 1, 1, 12, 0, 0 },
        v::time_point { 2001, 1, 1, 12, 0, 0 }), compare_result::equal);
    EXPECT_EQ(compare(
        v::time_point { 2001,  1,  1, 12, 0, 0 },
        v::time_point { 2000, 12, 31, 12, 0, 0 }), compare_result::greater);
    EXPECT_EQ(compare(
        v::time_point { 2001, 1, 1, 12, 0, 0 },
        v::time_point { 2001, 1, 2, 12, 0, 0 }), compare_result::less);
    EXPECT_EQ(compare(
        v::time_point { 2001, 1, 1, 12,  0,  0 },
        v::time_point { 2001, 1, 1, 11, 59, 59 }), compare_result::greater);
    EXPECT_EQ(compare(
        v::time_point { 2001, 1, 1, 12, 0, 0 },
        v::time_point { 2001, 1, 1, 12, 0, 1 }), compare_result::less);
    EXPECT_EQ(compare(
        v::time_point { 2001, 1, 1, 12, 0, 0, std::chrono::milliseconds(100) },
        v::time_point { 2001, 1, 1, 12, 0, 0, std::chrono::milliseconds(100) }), compare_result::equal);
    EXPECT_EQ(compare(
        v::time_point { 2001, 1, 1, 12, 0, 0, std::chrono::milliseconds(100) },
        v::time_point { 2001, 1, 1, 12, 0, 0, std::chrono::milliseconds( 99) }), compare_result::greater);
    EXPECT_EQ(compare(
        v::time_point { 2001, 1, 1, 12, 0, 0, std::chrono::milliseconds(100) },
        v::time_point { 2001, 1, 1, 12, 0, 0, std::chrono::milliseconds(101) }), compare_result::less);

    v::time_point value { 1970, 1, 1, 0, 0, 0 };

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 0, 0, 0 }), compare_result::undefined);
}

TEST_F(compare_value_test, unknown) {
    v::unknown value {};

    // uncomparable types
    EXPECT_EQ(compare(value, v::unknown {}), compare_result::undefined);
    EXPECT_EQ(compare(value, v::boolean { true }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int4 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::int8 { 10 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float4 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::float8 { 1.0 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::decimal { 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::character { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::octet { "A" }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::date { 1970, 1, 1 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_of_day { 12, 34, 56 }), compare_result::undefined);
    EXPECT_EQ(compare(value, v::time_point { 1970, 1, 1, 0, 0, 0 }), compare_result::undefined);
}

} // namespace yugawara::analyzer::details
