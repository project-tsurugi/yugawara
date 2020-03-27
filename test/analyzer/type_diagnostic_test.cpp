#include <yugawara/analyzer/type_diagnostic.h>

#include <gtest/gtest.h>

#include <takatori/document/basic_document.h>
#include <takatori/document/region.h>

#include <takatori/type/type_kind.h>
#include <takatori/type/character.h>

namespace yugawara::analyzer {

class type_diagnostic_test : public ::testing::Test {
public:
    ::takatori::document::basic_document doc {
            "testing.sql",
            "0123456789",
    };
};

namespace t = ::takatori::type;
using code = type_diagnostic::code_type;
using type::category;
using region = ::takatori::document::region;


TEST_F(type_diagnostic_test, simple) {
    type_diagnostic d {
            code::unsupported_type,
            region { doc, 0, 1 },
            t::character(t::varying),
            { category::number },
    };
    EXPECT_EQ(d.code(), code::unsupported_type);
    EXPECT_EQ(d.region(), region(doc, 0, 1));
    EXPECT_EQ(d.actual_type(), t::character(t::varying));
    EXPECT_EQ(d.expected_categories(), type::category_set({ category::number }));
}

TEST_F(type_diagnostic_test, output) {
    type_diagnostic d {
            code::unsupported_type,
            region { doc, 0, 1 },
            t::character(t::varying),
            { category::number, category::character_string },
    };
    std::cout << d << std::endl;
}

} // namespace yugawara::analyzer
