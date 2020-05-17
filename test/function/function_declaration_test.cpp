#include <yugawara/function/declaration.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/character.h>

namespace yugawara::function {

class function_declaration_test : public ::testing::Test {};

namespace t = ::takatori::type;

TEST_F(function_declaration_test, simple) {
    declaration d {
        1,
        "f",
        t::int4(),
        {
                t::int8(),
        },
    };

    EXPECT_EQ(d.definition_id(), 1);
    EXPECT_EQ(d.name(), "f");
    EXPECT_EQ(d.return_type(), t::int4());
    ASSERT_EQ(d.parameter_types().size(), 1);
    EXPECT_EQ(d.parameter_types()[0], t::int8());
}

TEST_F(function_declaration_test, has_wider_parameters_empty) {
    declaration a {
            1,
            "f",
            t::int4(),
            {},
    };
    declaration b {
            1,
            "f",
            t::int4(),
            {},
    };
    EXPECT_TRUE(a.has_wider_parameters(b));
    EXPECT_TRUE(b.has_wider_parameters(a));
}

TEST_F(function_declaration_test, has_wider_parameters_eq) {
    declaration a {
            1,
            "f",
            t::int4(),
            {
                    t::int4 {},
            },
    };
    declaration b {
            1,
            "f",
            t::int4(),
            {
                    t::int4 {},
            },
    };
    EXPECT_TRUE(a.has_wider_parameters(b));
    EXPECT_TRUE(b.has_wider_parameters(a));
}

TEST_F(function_declaration_test, has_wider_parameters_wider) {
    declaration a {
            1,
            "f",
            t::int4(),
            {
                    t::int8 {},
            },
    };
    declaration b {
            1,
            "f",
            t::int4(),
            {
                    t::int4 {},
            },
    };
    EXPECT_TRUE(a.has_wider_parameters(b));
    EXPECT_FALSE(b.has_wider_parameters(a));
}

TEST_F(function_declaration_test, has_wider_parameters_disjoint) {
    declaration a {
            1,
            "f",
            t::int4(),
            {
                    t::boolean {},
            },
    };
    declaration b {
            1,
            "f",
            t::int4(),
            {
                    t::int4 {},
            },
    };
    EXPECT_FALSE(a.has_wider_parameters(b));
    EXPECT_FALSE(b.has_wider_parameters(a));
}

TEST_F(function_declaration_test, output) {
    declaration d {
            1,
            "f",
            t::int4(),
            {
                    t::int8(),
                    t::character(t::varying),
            },
    };

    std::cout << d << std::endl;
}

} // namespace yugawara::function
