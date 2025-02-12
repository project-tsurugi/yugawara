#include <yugawara/type/comparable.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/bit.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>
#include <takatori/type/datetime_interval.h>
#include <takatori/type/lob.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

namespace yugawara::type {

class type_comparable_test : public ::testing::Test {};

namespace tt = ::takatori::type;

TEST_F(type_comparable_test, is_equality_comparable) {
    EXPECT_TRUE(is_equality_comparable(tt::boolean()));
    EXPECT_TRUE(is_equality_comparable(tt::int1()));
    EXPECT_TRUE(is_equality_comparable(tt::int2()));
    EXPECT_TRUE(is_equality_comparable(tt::int4()));
    EXPECT_TRUE(is_equality_comparable(tt::int8()));
    EXPECT_TRUE(is_equality_comparable(tt::decimal()));
    EXPECT_TRUE(is_equality_comparable(tt::float4()));
    EXPECT_TRUE(is_equality_comparable(tt::float8()));
    EXPECT_TRUE(is_equality_comparable(tt::character(tt::varying)));
    EXPECT_TRUE(is_equality_comparable(tt::character(~tt::varying, 100)));
    EXPECT_TRUE(is_equality_comparable(tt::bit(tt::varying)));
    EXPECT_TRUE(is_equality_comparable(tt::bit(~tt::varying, 100)));
    EXPECT_TRUE(is_equality_comparable(tt::octet(tt::varying)));
    EXPECT_TRUE(is_equality_comparable(tt::octet(~tt::varying, 100)));
    EXPECT_TRUE(is_equality_comparable(tt::date()));
    EXPECT_TRUE(is_equality_comparable(tt::time_of_day()));
    EXPECT_TRUE(is_equality_comparable(tt::time_of_day(tt::with_time_zone)));
    EXPECT_TRUE(is_equality_comparable(tt::time_point()));
    EXPECT_TRUE(is_equality_comparable(tt::time_point(tt::with_time_zone)));
    EXPECT_TRUE(is_equality_comparable(tt::datetime_interval()));
    EXPECT_FALSE(is_equality_comparable(tt::blob {}));
    EXPECT_FALSE(is_equality_comparable(tt::clob {}));
    EXPECT_TRUE(is_equality_comparable(tt::unknown()));
    EXPECT_FALSE(is_equality_comparable(extension::type::error()));
    EXPECT_FALSE(is_equality_comparable(extension::type::pending()));
}

TEST_F(type_comparable_test, is_order_comparable) {
    EXPECT_TRUE(is_order_comparable(tt::boolean()));
    EXPECT_TRUE(is_order_comparable(tt::int1()));
    EXPECT_TRUE(is_order_comparable(tt::int2()));
    EXPECT_TRUE(is_order_comparable(tt::int4()));
    EXPECT_TRUE(is_order_comparable(tt::int8()));
    EXPECT_TRUE(is_order_comparable(tt::decimal()));
    EXPECT_TRUE(is_order_comparable(tt::float4()));
    EXPECT_TRUE(is_order_comparable(tt::float8()));
    EXPECT_TRUE(is_order_comparable(tt::character(tt::varying)));
    EXPECT_TRUE(is_order_comparable(tt::character(~tt::varying, 100)));
    EXPECT_TRUE(is_order_comparable(tt::bit(tt::varying)));
    EXPECT_TRUE(is_order_comparable(tt::bit(~tt::varying, 100)));
    EXPECT_TRUE(is_order_comparable(tt::octet(tt::varying)));
    EXPECT_TRUE(is_order_comparable(tt::octet(~tt::varying, 100)));
    EXPECT_TRUE(is_order_comparable(tt::date()));
    EXPECT_TRUE(is_order_comparable(tt::time_of_day()));
    EXPECT_TRUE(is_order_comparable(tt::time_of_day(tt::with_time_zone)));
    EXPECT_TRUE(is_order_comparable(tt::time_point()));
    EXPECT_TRUE(is_order_comparable(tt::time_point(tt::with_time_zone)));
    EXPECT_FALSE(is_order_comparable(tt::datetime_interval()));
    EXPECT_FALSE(is_order_comparable(tt::blob {}));
    EXPECT_FALSE(is_order_comparable(tt::clob {}));
    EXPECT_FALSE(is_order_comparable(tt::unknown()));
    EXPECT_FALSE(is_order_comparable(extension::type::error()));
    EXPECT_FALSE(is_order_comparable(extension::type::pending()));
}

} // namespace yugawara::type
