#include <yugawara/function/declaration.h>

#include <gtest/gtest.h>

#include <takatori/type/int.h>
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
