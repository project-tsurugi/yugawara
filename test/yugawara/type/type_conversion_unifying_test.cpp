#include <yugawara/type/conversion.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>
#include <takatori/type/unknown.h>

#include <yugawara/extension/type/error.h>

namespace yugawara::type {

namespace tt = ::takatori::type;

class type_conversion_unifying_test : public ::testing::Test {
protected:
    void test(
            tt::data const& left,
            tt::data const& right,
            tt::data const& expected) {
        auto f_result = unifying_conversion(left, right);
        EXPECT_EQ(*f_result, expected) << "unify(" << left << ", " << right << ")";

        auto b_result = unifying_conversion(right, left);
        EXPECT_EQ(*b_result, expected) << "unify(" << right << ", " << left << ")";
    }

    extension::type::error const error {};
};

TEST_F(type_conversion_unifying_test, unknown) {
    test(tt::unknown {}, tt::unknown {}, tt::unknown {});
}

TEST_F(type_conversion_unifying_test, boolean) {
    test(tt::boolean {}, tt::boolean {}, tt::boolean {});
    test(tt::unknown {}, tt::boolean {}, tt::boolean {});
}

TEST_F(type_conversion_unifying_test, numeric_int1) {
    tt::int1 left {};

    test(left, tt::unknown {}, tt::int4 {});
    test(left, tt::int1 {}, tt::int4 {});
    test(left, tt::int2 {}, tt::int4 {});
    test(left, tt::int4 {}, tt::int4 {});
    test(left, tt::int8 {}, tt::int8 {});
    test(left, tt::decimal {  1,  0 }, tt::decimal { 10,  0 });
    test(left, tt::decimal { 15,  0 }, tt::decimal { 15,  0 });
    test(left, tt::decimal { 38,  0 }, tt::decimal { 38,  0 });
    test(left, tt::decimal { 38, 10 }, tt::decimal { {}, {} });
    test(left, tt::decimal { 20, {} }, tt::decimal { {}, {} });
    test(left, tt::decimal { {}, {} }, tt::decimal { {}, {} });
    test(left, tt::float4 {}, tt::float8 {});
    test(left, tt::float8 {}, tt::float8 {});
}

TEST_F(type_conversion_unifying_test, numeric_int2) {
    tt::int2 left {};

    test(left, tt::unknown {}, tt::int4 {});
    test(left, tt::int1 {}, tt::int4 {});
    test(left, tt::int2 {}, tt::int4 {});
    test(left, tt::int4 {}, tt::int4 {});
    test(left, tt::int8 {}, tt::int8 {});
    test(left, tt::decimal {  1,  0 }, tt::decimal { 10,  0 });
    test(left, tt::decimal { 15,  0 }, tt::decimal { 15,  0 });
    test(left, tt::decimal { 38,  0 }, tt::decimal { 38,  0 });
    test(left, tt::decimal { 38, 10 }, tt::decimal { {}, {} });
    test(left, tt::decimal { 20, {} }, tt::decimal { {}, {} });
    test(left, tt::decimal { {}, {} }, tt::decimal { {}, {} });
    test(left, tt::float4 {}, tt::float8 {});
    test(left, tt::float8 {}, tt::float8 {});
}

TEST_F(type_conversion_unifying_test, numeric_int4) {
    tt::int4 left {};

    test(left, tt::unknown {}, tt::int4 {});
    test(left, tt::int1 {}, tt::int4 {});
    test(left, tt::int2 {}, tt::int4 {});
    test(left, tt::int4 {}, tt::int4 {});
    test(left, tt::int8 {}, tt::int8 {});
    test(left, tt::decimal {  1,  0 }, tt::decimal { 10,  0 });
    test(left, tt::decimal { 15,  0 }, tt::decimal { 15,  0 });
    test(left, tt::decimal { 38,  0 }, tt::decimal { 38,  0 });
    test(left, tt::decimal { 38, 10 }, tt::decimal { {}, {} });
    test(left, tt::decimal { 20, {} }, tt::decimal { {}, {} });
    test(left, tt::decimal { {}, {} }, tt::decimal { {}, {} });
    test(left, tt::float4 {}, tt::float8 {});
    test(left, tt::float8 {}, tt::float8 {});
}

TEST_F(type_conversion_unifying_test, numeric_int8) {
    tt::int8 left {};

    test(left, tt::unknown {}, tt::int8 {});
    test(left, tt::int1 {}, tt::int8 {});
    test(left, tt::int2 {}, tt::int8 {});
    test(left, tt::int4 {}, tt::int8 {});
    test(left, tt::int8 {}, tt::int8 {});
    test(left, tt::decimal {  1,  0 }, tt::decimal { 19,  0 });
    test(left, tt::decimal { 15,  0 }, tt::decimal { 19,  0 });
    test(left, tt::decimal { 38,  0 }, tt::decimal { 38,  0 });
    test(left, tt::decimal { 38, 10 }, tt::decimal { {}, {} });
    test(left, tt::decimal { 20, {} }, tt::decimal { {}, {} });
    test(left, tt::decimal { {}, {} }, tt::decimal { {}, {} });
    test(left, tt::float4 {}, tt::float8 {});
    test(left, tt::float8 {}, tt::float8 {});
}

TEST_F(type_conversion_unifying_test, numeric_decimal) {
    tt::decimal left { 20, 0 };

    test(left, tt::unknown {}, tt::decimal { 20, 0 });
    test(left, tt::int1 {}, tt::decimal { 20, 0 });
    test(left, tt::int2 {}, tt::decimal { 20, 0 });
    test(left, tt::int4 {}, tt::decimal { 20, 0 });
    test(left, tt::int8 {}, tt::decimal { 20, 0 });
    test(left, tt::decimal { 10,  0 }, tt::decimal { 20,  0 });
    test(left, tt::decimal { 38,  0 }, tt::decimal { 38,  0 });
    test(left, tt::decimal { 38, 10 }, tt::decimal { {}, {} });
    test(left, tt::decimal { 20, {} }, tt::decimal { {}, {} });
    test(left, tt::decimal { {}, {} }, tt::decimal { {}, {} });
    test(left, tt::float4 {}, tt::float8 {});
    test(left, tt::float8 {}, tt::float8 {});
}

TEST_F(type_conversion_unifying_test, numeric_float4) {
    tt::float4 left {};

    test(left, tt::unknown {}, tt::float4 {});
    test(left, tt::int1 {}, tt::float8 {});
    test(left, tt::int2 {}, tt::float8 {});
    test(left, tt::int4 {}, tt::float8 {});
    test(left, tt::int8 {}, tt::float8 {});
    test(left, tt::decimal {  1,  0 }, tt::float8 {});
    test(left, tt::decimal { 15,  0 }, tt::float8 {});
    test(left, tt::decimal { 38,  0 }, tt::float8 {});
    test(left, tt::decimal { 38, 10 }, tt::float8 {});
    test(left, tt::decimal { 20, {} }, tt::float8 {});
    test(left, tt::decimal { {}, {} }, tt::float8 {});
    test(left, tt::float4 {}, tt::float4 {});
    test(left, tt::float8 {}, tt::float8 {});
}

TEST_F(type_conversion_unifying_test, character) {
    test(
            tt::character { ~tt::varying, 10 }, tt::unknown {},
            tt::character { tt::varying, 10 });
    test(
            tt::character { ~tt::varying, 10 }, tt::character { ~tt::varying, 10 },
            tt::character { tt::varying, 10 });
    test(
            tt::character { tt::varying, 10 }, tt::character { tt::varying, 10 },
            tt::character { tt::varying, 10 });
    test(
            tt::character { ~tt::varying, 10 }, tt::character { tt::varying, 10 },
            tt::character { tt::varying, 10 });

    test(
            tt::character { tt::varying, 10 }, tt::unknown {},
            tt::character { tt::varying, 10 });
    test(
            tt::character { ~tt::varying, 10 }, tt::character { ~tt::varying, 11 },
            tt::character { tt::varying, {} });
    test(
            tt::character { tt::varying, 10 }, tt::character { tt::varying, 11 },
            tt::character { tt::varying, {} });
    test(
            tt::character { ~tt::varying, 10 }, tt::character { tt::varying, 11 },
            tt::character { tt::varying, {} });
}

TEST_F(type_conversion_unifying_test, octet) {
    test(
            tt::octet { ~tt::varying, 10 }, tt::unknown {},
            tt::octet { tt::varying, 10 });
    test(
            tt::octet { ~tt::varying, 10 }, tt::octet { ~tt::varying, 10 },
            tt::octet { tt::varying, 10 });
    test(
            tt::octet { tt::varying, 10 }, tt::octet { tt::varying, 10 },
            tt::octet { tt::varying, 10 });
    test(
            tt::octet { ~tt::varying, 10 }, tt::octet { tt::varying, 10 },
            tt::octet { tt::varying, 10 });

    test(
            tt::octet { tt::varying, 10 }, tt::unknown {},
            tt::octet { tt::varying, 10 });
    test(
            tt::octet { ~tt::varying, 10 }, tt::octet { ~tt::varying, 11 },
            tt::octet { tt::varying, {} });
    test(
            tt::octet { tt::varying, 10 }, tt::octet { tt::varying, 11 },
            tt::octet { tt::varying, {} });
    test(
            tt::octet { ~tt::varying, 10 }, tt::octet { tt::varying, 11 },
            tt::octet { tt::varying, {} });
}

TEST_F(type_conversion_unifying_test, date) {
    tt::date left {};

    test(left, tt::unknown {}, left);
    test(left, tt::date {}, left);
    test(left, tt::time_of_day { ~tt::with_time_zone }, error);
    test(left, tt::time_of_day { tt::with_time_zone }, error);
    test(left, tt::time_point { ~tt::with_time_zone }, error);
    test(left, tt::time_point { tt::with_time_zone }, error);
}

TEST_F(type_conversion_unifying_test, time_of_day) {
    tt::time_of_day left { ~tt::with_time_zone };

    test(left, tt::unknown {}, left);
    test(left, tt::date {}, error);
    test(left, tt::time_of_day { ~tt::with_time_zone }, left);
    test(left, tt::time_of_day { tt::with_time_zone }, error);
    test(left, tt::time_point { ~tt::with_time_zone }, error);
    test(left, tt::time_point { tt::with_time_zone }, error);
}

TEST_F(type_conversion_unifying_test, timet_of_day_tz) {
    tt::time_of_day left { tt::with_time_zone };

    test(left, tt::unknown {}, left);
    test(left, tt::date {}, error);
    test(left, tt::time_of_day { ~tt::with_time_zone }, error);
    test(left, tt::time_of_day { tt::with_time_zone }, left);
    test(left, tt::time_point { ~tt::with_time_zone }, error);
    test(left, tt::time_point { tt::with_time_zone }, error);
}

TEST_F(type_conversion_unifying_test, time_point) {
    tt::time_point left { ~tt::with_time_zone };

    test(left, tt::unknown {}, left);
    test(left, tt::date {}, error);
    test(left, tt::time_of_day { ~tt::with_time_zone }, error);
    test(left, tt::time_of_day { tt::with_time_zone }, error);
    test(left, tt::time_point { ~tt::with_time_zone }, left);
    test(left, tt::time_point { tt::with_time_zone }, error);
}

TEST_F(type_conversion_unifying_test, time_point_tz) {
    tt::time_point left { tt::with_time_zone };

    test(left, tt::unknown {}, left);
    test(left, tt::date {}, error);
    test(left, tt::time_of_day { ~tt::with_time_zone }, error);
    test(left, tt::time_of_day { tt::with_time_zone }, error);
    test(left, tt::time_point { ~tt::with_time_zone }, error);
    test(left, tt::time_point { tt::with_time_zone }, left);
}

} // namespace yugawara::type
