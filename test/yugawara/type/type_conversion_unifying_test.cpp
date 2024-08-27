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
};

TEST_F(type_conversion_unifying_test, unknown) {
    test(tt::unknown {}, tt::unknown {}, tt::unknown {});
}

TEST_F(type_conversion_unifying_test, boolean) {
    test(tt::boolean {}, tt::boolean {}, tt::boolean {});

    test(tt::boolean {}, tt::unknown {}, tt::boolean {});
    test(tt::unknown {}, tt::boolean {}, tt::boolean {});
}

TEST_F(type_conversion_unifying_test, numeric_decimal) {
    tt::decimal left { 20, 0 };

    test(left, tt::int1 {}, tt::decimal { 20, 0 });
    test(left, tt::int2 {}, tt::decimal { 20, 0 });
    test(left, tt::int4 {}, tt::decimal { 20, 0 });
    test(left, tt::int8 {}, tt::decimal { 20, 0 });
    test(left, tt::decimal { 10,  0 }, tt::decimal { 20,  0 });
    test(left, tt::decimal { 38,  0 }, tt::decimal { 38,  0 });
    test(left, tt::decimal { 38, 10 }, tt::decimal { {}, {} });
    test(left, tt::decimal { 20, {} }, tt::decimal { {}, {} });
    test(left, tt::decimal { {}, {} }, tt::decimal { {}, {} });
}

} // namespace yugawara::type
