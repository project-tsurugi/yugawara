#include <yugawara/type/conversion.h>

#include <gtest/gtest.h>

#include <takatori/type/boolean.h>
#include <takatori/type/int.h>
#include <takatori/type/decimal.h>
#include <takatori/type/float.h>
#include <takatori/type/character.h>
#include <takatori/type/bit.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>
#include <takatori/type/unknown.h>

namespace yugawara::type {

namespace tt = ::takatori::type;

using ::yugawara::util::ternary;

class type_conversion_overload_test : public ::testing::Test {};

TEST_F(type_conversion_overload_test, is_most_upperbound_compatible_type_for_int4) {
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::int1 {}), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::int2 {}), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::int4 {}), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_most_upperbound_compatible_type_for_decimal) {
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::decimal { 38, 10 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::decimal { 10, {} }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::decimal { {}, 2 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::decimal { {}, {} }), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_most_upperbound_compatible_type_for_character) {
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::character { {} }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::character { 10 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::character { tt::varying, 10 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::character { tt::varying, {} }), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_most_upperbound_compatible_type_for_bit) {
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::bit { {} }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::bit { 10 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::bit { tt::varying, 10 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::bit { tt::varying, {} }), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_most_upperbound_compatible_type_for_octet) {
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::octet { {} }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::octet { 10 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::octet { tt::varying, 10 }), ternary::no);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::octet { tt::varying, {} }), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_most_upperbound_compatible_type_for_itself) {
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::boolean {}), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::float8 {}), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::date {}), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::time_of_day {}), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::time_of_day { tt::with_time_zone }), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::time_point {}), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::time_point { tt::with_time_zone }), ternary::yes);
    EXPECT_EQ(is_most_upperbound_compatible_type(tt::unknown {}), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_boolean) {
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::boolean {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::boolean {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_int4) {
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::int4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::int8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::float8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int4 {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_int8) {
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::int8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::float8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::int8 {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_decimal) {
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::float8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_float4) {
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::float8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float4 {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_float8) {
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::float8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::float8 {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_character) {
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::decimal {{}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::character { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::time_of_day { tt::with_time_zone}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::time_point { tt::with_time_zone}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_bit) {
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::decimal {{}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::bit { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::time_of_day { tt::with_time_zone}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::time_point { tt::with_time_zone}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_octet) {
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::decimal {{}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::octet { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::time_of_day { tt::with_time_zone}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::time_point { tt::with_time_zone}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_date) {
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::date {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::date {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_time_of_day) {
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::time_of_day {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_time_of_day_tz) {
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::time_of_day { tt::with_time_zone }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_of_day { tt::with_time_zone }, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_time_point) {
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::time_point {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::time_point { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point {}, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_time_point_tz) {
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::boolean {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::int4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::int8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::decimal { {}, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::float4 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::float8 {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::character { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::bit { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::octet { tt::varying, {} }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::date {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::time_of_day {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::time_of_day { tt::with_time_zone }), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::time_point {}), ternary::no);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::time_point { tt::with_time_zone }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::time_point { tt::with_time_zone }, tt::unknown {}), ternary::no);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_unknown) {
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::boolean {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::int4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::int8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::float8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::character { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::bit { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::octet { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::date {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::time_of_day {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::time_of_day { tt::with_time_zone }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::time_point {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::time_point { tt::with_time_zone }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::unknown {}, tt::unknown {}), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_noub_int1) {
    EXPECT_EQ(is_parameter_application_convertible(tt::int1 {}, tt::int4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int1 {}, tt::int8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int1 {}, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int1 {}, tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int1 {}, tt::float8 {}), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_noub_int2) {
    EXPECT_EQ(is_parameter_application_convertible(tt::int2 {}, tt::int4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int2 {}, tt::int8 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int2 {}, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int2 {}, tt::float4 {}), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::int2 {}, tt::float8 {}), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_noub_decimal) {
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { 38, 10 }, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { 10, {} }, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {},  2 }, tt::decimal { {}, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::decimal { {}, {} }, tt::decimal { {}, {} }), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_noub_character) {
    EXPECT_EQ(is_parameter_application_convertible(tt::character { 10 }, tt::character { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, 10 }, tt::character { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::character { tt::varying, {} }, tt::character { tt::varying, {} }), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_noub_bit) {
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { 10 }, tt::bit { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, 10 }, tt::bit { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::bit { tt::varying, {} }, tt::bit { tt::varying, {} }), ternary::yes);
}

TEST_F(type_conversion_overload_test, is_parameter_application_convertible_for_noub_octet) {
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { 10 }, tt::octet { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, 10 }, tt::octet { tt::varying, {} }), ternary::yes);
    EXPECT_EQ(is_parameter_application_convertible(tt::octet { tt::varying, {} }, tt::octet { tt::varying, {} }), ternary::yes);
}

} // namespace yugawara::type
