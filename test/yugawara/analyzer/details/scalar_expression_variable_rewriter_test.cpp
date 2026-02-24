#include <yugawara/analyzer/details/scalar_expression_variable_rewriter.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/let.h>
#include <takatori/scalar/binary.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>

#include <yugawara/binding/factory.h>

#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/relation/subquery.h>

#include <yugawara/type/repository.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class scalar_expression_variable_rewriter_test : public ::testing::Test {
protected:
    type::repository types;
    binding::factory bindings;

    void check_context(stream_variable_rewriter_context& context) {
        context.each_undefined([](auto& variable) {
            ADD_FAILURE() << "undefined variable: " << variable;
        });
    }
};

TEST_F(scalar_expression_variable_rewriter_test, simple) {
    scalar::immediate expr {
            v::int4 {},
            t::int4 {},
    };
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
    rewriter(context, expr);
    check_context(context);
    // ok.
}

TEST_F(scalar_expression_variable_rewriter_test, variable_reference) {
    auto c0 = bindings.stream_variable("c0");
    scalar::variable_reference e0 { c0 };
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
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
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
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
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
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
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
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
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
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

TEST_F(scalar_expression_variable_rewriter_test, scalar_subquery) {
    /*
     * values:r0 -*
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    extension::scalar::subquery e0 {
            std::move(g0),
            c0,
    };
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
    rewriter(context, e0);
    check_context(context);

    auto c0m = context.find(c0);
    EXPECT_NE(c0, c0m);

    EXPECT_EQ(e0.output_column(), c0m);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0], c0m);
}

TEST_F(scalar_expression_variable_rewriter_test, scalar_subquery_nesting) {
    /*
     * values:r0 -*
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    relation::graph_type g1;
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g1.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { c0, c1 },
            },
            true,
    });

    extension::scalar::subquery e0 {
            std::move(g1),
            c1,
    };
    stream_variable_rewriter_context context {};
    scalar_expression_variable_rewriter rewriter {};
    rewriter(context, e0);
    check_context(context);

    EXPECT_FALSE(context.find(c0));
    auto c1m = context.find(c1);
    ASSERT_TRUE(c1m);;
    EXPECT_NE(c1m, c1);

    EXPECT_EQ(e0.output_column(), c1m);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0], c0);

    ASSERT_EQ(r1.mappings().size(), 1);
    EXPECT_EQ(r1.mappings()[0].source(), c0);
    EXPECT_EQ(r1.mappings()[0].destination(), c1m);
}

} // namespace yugawara::analyzer::details
