#include <yugawara/type/category.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/bit.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>
#include <takatori/type/datetime_interval.h>

#include <yugawara/type/extensions/error.h>
#include <yugawara/type/extensions/pending.h>

#include "dummy_extension.h"

namespace yugawara::type {

class type_category_test : public ::testing::Test {};

namespace tt = ::takatori::type;
using ::takatori::datetime::time_zone;

TEST_F(type_category_test, basics) {
    EXPECT_EQ(category_of(tt::boolean()), category::boolean);

    EXPECT_EQ(category_of(tt::int1()), category::number);
    EXPECT_EQ(category_of(tt::int2()), category::number);
    EXPECT_EQ(category_of(tt::int4()), category::number);
    EXPECT_EQ(category_of(tt::int8()), category::number);

    EXPECT_EQ(category_of(tt::decimal()), category::number);
    EXPECT_EQ(category_of(tt::decimal(10, 2)), category::number);

    EXPECT_EQ(category_of(tt::float4()), category::number);
    EXPECT_EQ(category_of(tt::float8()), category::number);

    EXPECT_EQ(category_of(tt::character(tt::varying)), category::character_string);
    EXPECT_EQ(category_of(tt::character(~tt::varying, 100)), category::character_string);

    EXPECT_EQ(category_of(tt::bit(tt::varying)), category::bit_string);
    EXPECT_EQ(category_of(tt::bit(~tt::varying, 100)), category::bit_string);

    EXPECT_EQ(category_of(tt::date()), category::temporal);
    EXPECT_EQ(category_of(tt::time_of_day()), category::temporal);
    EXPECT_EQ(category_of(tt::time_of_day(time_zone::UTC)), category::temporal);
    EXPECT_EQ(category_of(tt::time_point()), category::temporal);
    EXPECT_EQ(category_of(tt::time_point(time_zone::UTC)), category::temporal);

    EXPECT_EQ(category_of(tt::datetime_interval()), category::datetime_interval);
}

TEST_F(type_category_test, unknown) {
    EXPECT_EQ(category_of(tt::unknown()), category::unknown);
}

TEST_F(type_category_test, unresolved) {
    EXPECT_EQ(category_of(extensions::error()), category::unresolved);
    EXPECT_EQ(category_of(extensions::pending()), category::unresolved);
}

TEST_F(type_category_test, external) {
    EXPECT_EQ(category_of(dummy_extension<1>()), category::external);
}

} // namespace yugawara::type
