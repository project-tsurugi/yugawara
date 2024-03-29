#include <yugawara/variable/declaration.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>

#include <yugawara/extension/type/pending.h>

namespace yugawara::variable {

class variable_declaration_test : public ::testing::Test {};

namespace t = takatori::type;
namespace v = takatori::value;

TEST_F(variable_declaration_test, unresolved) {
    declaration d {
        "?"
    };

    EXPECT_FALSE(d.is_resolved());
    EXPECT_EQ(d.name(), "?");
    EXPECT_EQ(d.type(), extension::type::pending());
    EXPECT_EQ(d.criteria().nullity(), nullable);
}

TEST_F(variable_declaration_test, resolved) {
    declaration d {
            "v",
            t::int4(),
    };

    EXPECT_TRUE(d);
    EXPECT_EQ(d.name(), "v");
    EXPECT_EQ(d.type(), t::int4());
    EXPECT_EQ(d.criteria().nullity(), nullable);
}

TEST_F(variable_declaration_test, criteria) {
    declaration d {
            "v",
            t::int4(),
            v::int4(100),
    };

    EXPECT_TRUE(d);
    EXPECT_EQ(d.name(), "v");
    EXPECT_EQ(d.type(), t::int4());
    EXPECT_EQ(d.criteria().constant(), v::int4(100));
}

TEST_F(variable_declaration_test, output) {
    declaration d {
            "v",
            t::int4(),
            v::int4(100),
    };

    std::cout << d << std::endl;
}

} // namespace yugawara::variable
