#include <yugawara/analyzer/type_diagnostic.h>

#include <gtest/gtest.h>

#include <takatori/type/type_kind.h>
#include <takatori/type/character.h>
#include <takatori/value/int.h>
#include <takatori/scalar/immediate.h>

#include <yugawara/type/repository.h>

namespace yugawara::analyzer {

class type_diagnostic_test : public ::testing::Test {};

namespace s = ::takatori::scalar;
namespace t = ::takatori::type;
namespace v = ::takatori::value;
using code = type_diagnostic::code_type;
using cat = type::category;

TEST_F(type_diagnostic_test, simple) {
    s::immediate expr {
            v::int4(100),
            t::character(t::varying),
    };
    type_diagnostic d {
            code::unsupported_type,
            expr,
            t::character(t::varying),
            { cat::number },
    };
    EXPECT_EQ(d.code(), code::unsupported_type);
    EXPECT_EQ(std::addressof(d.location()), std::addressof(expr));
    EXPECT_EQ(d.actual_type(), t::character(t::varying));
    EXPECT_EQ(d.expected_categories(), type_diagnostic::category_set({ cat::number }));
}

TEST_F(type_diagnostic_test, output) {
    s::immediate expr {
            v::int4(100),
            t::character(t::varying),
    };
    type_diagnostic d {
            code::unsupported_type,
            expr,
            t::character(t::varying),
            { cat::number, cat::character_string },
    };
    std::cout << d << std::endl;
}

} // namespace yugawara::analyzer
