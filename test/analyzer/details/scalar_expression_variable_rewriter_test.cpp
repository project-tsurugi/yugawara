#include <analyzer/details/scalar_expression_variable_rewriter.h>

#include <gtest/gtest.h>

#include <takatori/type/int.h>
#include <takatori/value/int.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/let.h>
#include <takatori/scalar/binary.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>

#include "utils.h"

namespace yugawara::analyzer::details {

class scalar_expression_variable_rewriter_test : public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    type::repository types;
    binding::factory bindings;
};

TEST_F(scalar_expression_variable_rewriter_test, simple) {
    scalar::immediate expr {
            v::int4 {},
            t::int4 {},
    };
    stream_variable_rewriter_context context { creator };
    scalar_expression_variable_rewriter rewriter { creator };
    rewriter(context, expr);
    // ok.
}

TEST_F(scalar_expression_variable_rewriter_test, variable_reference) {
    auto c0 = bindings.stream_variable("c0");
    scalar::variable_reference e0 { c0 };
    stream_variable_rewriter_context context { creator };
    scalar_expression_variable_rewriter rewriter { creator };
    rewriter(context, e0);

    auto c0m = context.find(c0);
    EXPECT_NE(c0, c0m);
    EXPECT_EQ(e0.variable(), c0m);
}

TEST_F(scalar_expression_variable_rewriter_test, variable_reference_multiple) {
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    scalar::variable_reference e0 { c0 };
    scalar::variable_reference e1 { c1 };
    scalar::variable_reference e2 { c0 };
    stream_variable_rewriter_context context { creator };
    scalar_expression_variable_rewriter rewriter { creator };
    rewriter(context, e0);
    rewriter(context, e1);
    rewriter(context, e2);

    auto c0m = context.find(c0);
    auto c1m = context.find(c1);
    EXPECT_EQ(e0.variable(), c0m);
    EXPECT_EQ(e1.variable(), c1m);
    EXPECT_EQ(e2.variable(), c0m);
}

TEST_F(scalar_expression_variable_rewriter_test, let) {
    auto l0 = bindings.local_variable("l0");
    scalar::let e0 {
            scalar::let::declarator {
                    l0,
                    scalar::immediate {
                           v::int4 {},
                            t::int4 {},
                    },
            },
            scalar::variable_reference { l0 },
    };
    stream_variable_rewriter_context context { creator };
    scalar_expression_variable_rewriter rewriter { creator };
    rewriter(context, e0);

    auto&& l0m = e0.variables()[0].variable();
    auto&& vref = downcast<scalar::variable_reference>(e0.body());

    EXPECT_NE(l0m, l0);
    EXPECT_EQ(vref.variable(), l0m);
}

TEST_F(scalar_expression_variable_rewriter_test, let_multiple) {
    auto l0 = bindings.local_variable("l0");
    auto l1 = bindings.local_variable("l1");
    auto l2 = bindings.local_variable("l2");
    auto c0 = bindings.stream_variable("c0");
    scalar::let e0 {
            {
                    scalar::let::declarator {
                            l0,
                            scalar::variable_reference { c0 },
                    },
                    scalar::let::declarator {
                            l1,
                            scalar::variable_reference { l0 },
                    },
                    scalar::let::declarator {
                            l2,
                            scalar::variable_reference { l1 },
                    },
            },
            scalar::variable_reference { l2 },
    };
    stream_variable_rewriter_context context { creator };
    scalar_expression_variable_rewriter rewriter { creator };
    rewriter(context, e0);

    auto c0m = context.find(c0);
    auto&& l0m = e0.variables()[0].variable();
    auto&& l1m = e0.variables()[1].variable();
    auto&& l2m = e0.variables()[2].variable();

    auto&& v0 = downcast<scalar::variable_reference>(e0.variables()[0].value());
    auto&& v1 = downcast<scalar::variable_reference>(e0.variables()[1].value());
    auto&& v2 = downcast<scalar::variable_reference>(e0.variables()[2].value());
    auto&& vb = downcast<scalar::variable_reference>(e0.body());

    EXPECT_NE(c0m, c0);
    EXPECT_NE(l0m, l0);
    EXPECT_NE(l1m, l1);
    EXPECT_NE(l2m, l2);

    EXPECT_EQ(v0.variable(), c0m);
    EXPECT_EQ(v1.variable(), l0m);
    EXPECT_EQ(v2.variable(), l1m);
    EXPECT_EQ(vb.variable(), l2m);
}

TEST_F(scalar_expression_variable_rewriter_test, nesting) {
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");

    scalar::binary e0 {
            scalar::binary_operator::add,
            scalar::variable_reference { c0 },
            scalar::variable_reference { c1 },
    };
    stream_variable_rewriter_context context { creator };
    scalar_expression_variable_rewriter rewriter { creator };
    rewriter(context, e0);

    auto&& v0 = downcast<scalar::variable_reference>(e0.left());
    auto&& v1 = downcast<scalar::variable_reference>(e0.right());

    auto c0m = context.find(c0);
    auto c1m = context.find(c1);
    EXPECT_NE(c0, c0m);
    EXPECT_NE(c1, c1m);
    EXPECT_EQ(v0.variable(), c0m);
    EXPECT_EQ(v1.variable(), c1m);
}

} // namespace yugawara::analyzer::details
