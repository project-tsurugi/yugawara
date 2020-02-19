#include <yugawara/storage/table.h>

#include <gtest/gtest.h>

#include <takatori/type/int.h>
#include <takatori/type/character.h>

namespace yugawara::storage {

class table_test : public ::testing::Test {};

static_assert(table::tag == relation_kind::table);

namespace t = ::takatori::type;
namespace v = ::takatori::value;

TEST_F(table_test, simple) {
    table t {
            "T1",
            {
                    { "C1", t::int4() },
            },
    };

    EXPECT_EQ(t.simple_name(), "T1");
    ASSERT_EQ(t.columns().size(), 1);
    {
        auto&& c = t.columns()[0];
        EXPECT_EQ(c.simple_name(), "C1");
        EXPECT_EQ(c.type(), t::int4());
    }
}

TEST_F(table_test, multiple_columns) {
    table t {
            "T1",
            {
                    { "C1", t::int4() },
                    { "C2", t::int8() },
                    { "C3", t::character(t::varying) },
            },
    };

    EXPECT_EQ(t.simple_name(), "T1");
    ASSERT_EQ(t.columns().size(), 3);
    {
        auto&& c = t.columns()[0];
        EXPECT_EQ(c.simple_name(), "C1");
        EXPECT_EQ(c.type(), t::int4());
    }
    {
        auto&& c = t.columns()[1];
        EXPECT_EQ(c.simple_name(), "C2");
        EXPECT_EQ(c.type(), t::int8());
    }
    {
        auto&& c = t.columns()[2];
        EXPECT_EQ(c.simple_name(), "C3");
        EXPECT_EQ(c.type(), t::character(t::varying));
    }
}

TEST_F(table_test, output) {
    table t {
            "T1",
            {
                    { "C1", t::int4() },
                    { "C2", t::int8() },
                    { "C3", t::character(t::varying) },
            },
    };
    std::cout << t << std::endl;
}

} // namespace yugawara::storage
