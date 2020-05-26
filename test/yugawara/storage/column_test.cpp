#include <yugawara/storage/column.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>

namespace yugawara::storage {

class column_test : public ::testing::Test {};

namespace t = ::takatori::type;
namespace v = ::takatori::value;

TEST_F(column_test, simple) {
    column c {
            "C1",
            t::int4(),
    };

    EXPECT_EQ(c.simple_name(), "C1");
    EXPECT_EQ(c.type(), t::int4());
    EXPECT_EQ(c.default_value(), nullptr);
    EXPECT_EQ(c.criteria().nullity(), variable::nullable);
}

TEST_F(column_test, criteria) {
    column c {
            "C1",
            t::int4(),
            ~variable::nullable,
    };

    EXPECT_EQ(c.criteria().nullity(), ~variable::nullable);
}

TEST_F(column_test, default_value) {
    column c {
            "C1",
            t::int4(),
            {},
            v::int4(100),
    };

    EXPECT_EQ(c.default_value(), v::int4(100));
}

TEST_F(column_test, output) {
    column c {
            "C1",
            t::int4(),
            ~variable::nullable,
            v::int4(100),
    };
    std::cout << c << std::endl;
}

} // namespace yugawara::storage
