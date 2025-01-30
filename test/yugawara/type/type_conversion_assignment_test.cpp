#include <yugawara/type/conversion.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>
#include <takatori/type/lob.h>

namespace yugawara::type {

namespace tt = ::takatori::type;

class type_conversion_assignment_test : public ::testing::Test {
protected:
    static constexpr util::ternary yes = util::ternary::yes;
    static constexpr util::ternary no = util::ternary::no;
};

TEST_F(type_conversion_assignment_test, boolean) {
    tt::boolean left {};
    
    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, int1) {
    tt::int1 left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, int2) {
    tt::int2 left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, int4) {
    tt::int4 left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, int8) {
    tt::int8 left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, decimal) {
    tt::decimal left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, float4) {
    tt::float4 left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, float8) {
    tt::float8 left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, character) {
    tt::character left { ~tt::varying };

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, character_varying) {
    tt::character left { tt::varying };

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, octet) {
    tt::octet left { ~tt::varying };

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, octet_varying) {
    tt::octet left { tt::varying };

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, date) {
    tt::date left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, time_of_day) {
    tt::time_of_day left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, time_of_day_with_time_zone) {
    tt::time_of_day left { tt::with_time_zone };

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, time_point) {
    tt::time_point left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, time_point_with_time_zone) {
    tt::time_point left { tt::with_time_zone };

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, blob) {
    tt::blob left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), no);
}

TEST_F(type_conversion_assignment_test, clob) {
    tt::clob left {};

    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), no);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), yes);
}

TEST_F(type_conversion_assignment_test, unknown) {
    tt::unknown left {};
    
    EXPECT_EQ(is_assignment_convertible(left, tt::boolean {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int1 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int2 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::int8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float4 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::float8 {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::decimal {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { ~tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::character { tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { ~tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::octet { tt::varying }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::date {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { ~tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_of_day { tt::with_time_zone}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { ~tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::time_point { tt::with_time_zone }), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::blob {}), yes);
    EXPECT_EQ(is_assignment_convertible(left, tt::clob {}), yes);
}

} // namespace yugawara::type
