#include <yugawara/analyzer/details/rewrite_stream_variables.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>

#include <yugawara/binding/factory.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class rewrite_stream_variables_subquery_test : public ::testing::Test {
protected:
    binding::factory bindings;
};

TEST_F(rewrite_stream_variables_subquery_test, simple) {
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

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c0, s0 },
            },
    };

    rewrite_stream_variables(q0);

    auto c0m = q0.mappings()[0].source();
    EXPECT_NE(c0m, c0);

    EXPECT_EQ(q0.mappings()[0].destination(), s0);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0], c0m);
}

TEST_F(rewrite_stream_variables_subquery_test, join) {
    /*
     * values:i0 -\
     *             +-- join:r0 -*
     * values:i1 -/
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& i0 = g0.insert(relation::values {
            { c0 },
            {},
    });
    auto&& i1 = g0.insert(relation::values {
            { c1 },
            {},
    });
    auto&& r0 = g0.insert(relation::intermediate::join {
            relation::join_kind::inner,
            {
                    compare(c0, c1),
            },
    });
    i0.output() >> r0.left();
    i1.output() >> r0.right();

    auto s0 = bindings.stream_variable("s0");
    auto s1 = bindings.stream_variable("s1");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c0, s0 },
                    { c1, s1 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 2);
    auto c0m = q0.mappings()[0].source();
    auto c1m = q0.mappings()[1].source();
    EXPECT_NE(c0m, c0);
    EXPECT_NE(c1m, c1);

    EXPECT_EQ(r0.condition(), compare(c0m, c1m));
}

TEST_F(rewrite_stream_variables_subquery_test, aggregate) {
    auto f = bindings.aggregate_function(aggregate::declaration {
            1,
            "f",
            takatori::type::int4 {},
            {
                    takatori::type::int4 {},
            },
            false,
    });

    /*
     * values:i0 -- aggregate:r0 -*
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1, c2 },
            {},
    });
    auto c4 = bindings.stream_variable("c4");
    auto c5 = bindings.stream_variable("c5");
    auto&& r0 = g0.insert(relation::intermediate::aggregate {
            { c0 },
            {
                    { f, { c1 }, c4 },
                    { f, { c2 }, c5 },
            },

    });
    i0.output() >> r0.input();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c4, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c4m = q0.mappings()[0].source();
    EXPECT_NE(c4m, c4);

    ASSERT_EQ(r0.group_keys().size(), 1);
    auto c0m = r0.group_keys()[0];
    EXPECT_NE(c0m, c0);

    ASSERT_EQ(r0.columns().size(), 1);

    EXPECT_EQ(r0.columns()[0].destination(), c4m);
    ASSERT_EQ(r0.columns()[0].arguments().size(), 1);
    auto c1m = r0.columns()[0].arguments()[0];
    EXPECT_NE(c1m, c1);

    ASSERT_EQ(i0.columns().size(), 2);
    EXPECT_EQ(i0.columns()[0], c0m);
    EXPECT_EQ(i0.columns()[1], c1m);
}

TEST_F(rewrite_stream_variables_subquery_test, distinct) {
    auto f = bindings.aggregate_function(aggregate::declaration {
            1,
            "f",
            takatori::type::int4 {},
            {
                    takatori::type::int4 {},
            },
            false,
    });

    /*
     * values:i0 -- distinct:r0 -*
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1, c2 },
            {},
    });
    auto&& r0 = g0.insert(relation::intermediate::distinct {
            { c0 },
    });
    i0.output() >> r0.input();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c1, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c1m = q0.mappings()[0].source();
    EXPECT_NE(c1m, c1);

    ASSERT_EQ(r0.group_keys().size(), 1);
    auto c0m = r0.group_keys()[0];
    EXPECT_NE(c0m, c0);

    ASSERT_EQ(i0.columns().size(), 2);
    EXPECT_EQ(i0.columns()[0], c0m);
    EXPECT_EQ(i0.columns()[1], c1m);
}

TEST_F(rewrite_stream_variables_subquery_test, limit) {
    auto f = bindings.aggregate_function(aggregate::declaration {
            1,
            "f",
            takatori::type::int4 {},
            {
                    takatori::type::int4 {},
            },
            false,
    });

    /*
     * values:i0 -- limit:r0 -*
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1, c2, c3 },
            {},
    });
    auto&& r0 = g0.insert(relation::intermediate::limit {
            {},
            { c0 },
            { { c1 } },
    });
    i0.output() >> r0.input();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c2, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c2m = q0.mappings()[0].source();
    EXPECT_NE(c2m, c2);

    ASSERT_EQ(r0.group_keys().size(), 1);
    auto c0m = r0.group_keys()[0];
    EXPECT_NE(c0m, c0);

    ASSERT_EQ(r0.sort_keys().size(), 1);
    auto c1m = r0.sort_keys()[0].variable();
    EXPECT_NE(c1m, c1);

    ASSERT_EQ(i0.columns().size(), 3);
    EXPECT_EQ(i0.columns()[0], c0m);
    EXPECT_EQ(i0.columns()[1], c1m);
    EXPECT_EQ(i0.columns()[2], c2m);
}

TEST_F(rewrite_stream_variables_subquery_test, union_all) {
    /*
     * values:i0 -\
     *             +-- union:r0 -*
     * values:i1 -/
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1 },
            {},
    });
    auto&& i1 = g0.insert(relation::values {
            { c2, c3 },
            {},
    });

    auto c4 = bindings.stream_variable("c4");
    auto c5 = bindings.stream_variable("c5");
    auto&& r0 = g0.insert(relation::intermediate::union_ {
            {
                    { c0, c2, c4 },
                    { c1, c3, c5 },
            },
            relation::set_quantifier::all,
    });
    i0.output() >> r0.left();
    i1.output() >> r0.right();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c4, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c4m = q0.mappings()[0].source();
    EXPECT_NE(c4m, c4);

    ASSERT_EQ(r0.mappings().size(), 1);
    auto c0m = r0.mappings()[0].left();
    auto c2m = r0.mappings()[0].right();
    EXPECT_NE(c0m, c0);
    EXPECT_NE(c2m, c2);

    ASSERT_EQ(i0.columns().size(), 1);
    EXPECT_EQ(i0.columns()[0], c0m);

    ASSERT_EQ(i1.columns().size(), 1);
    EXPECT_EQ(i1.columns()[0], c2m);
}

TEST_F(rewrite_stream_variables_subquery_test, union_distinct) {
    /*
     * values:i0 -\
     *             +-- union:r0 -*
     * values:i1 -/
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1 },
            {},
    });
    auto&& i1 = g0.insert(relation::values {
            { c2, c3 },
            {},
    });

    auto c4 = bindings.stream_variable("c4");
    auto c5 = bindings.stream_variable("c5");
    auto&& r0 = g0.insert(relation::intermediate::union_ {
            {
                    { c0, c2, c4 },
                    { c1, c3, c5 },
            },
            relation::set_quantifier::distinct,
    });
    i0.output() >> r0.left();
    i1.output() >> r0.right();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c4, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c4m = q0.mappings()[0].source();
    EXPECT_NE(c4m, c4);

    ASSERT_EQ(r0.mappings().size(), 2);
    auto c0m = r0.mappings()[0].left();
    auto c1m = r0.mappings()[1].left();
    auto c2m = r0.mappings()[0].right();
    auto c3m = r0.mappings()[1].right();
    EXPECT_NE(c0m, c0);
    EXPECT_NE(c1m, c1);
    EXPECT_NE(c2m, c2);
    EXPECT_NE(c3m, c3);

    ASSERT_EQ(i0.columns().size(), 2);
    EXPECT_EQ(i0.columns()[0], c0m);
    EXPECT_EQ(i0.columns()[1], c1m);

    ASSERT_EQ(i1.columns().size(), 2);
    EXPECT_EQ(i1.columns()[0], c2m);
    EXPECT_EQ(i1.columns()[1], c3m);
}

TEST_F(rewrite_stream_variables_subquery_test, intersection) {
    /*
     * values:i0 -\
     *             +-- intersection:r0 -*
     * values:i1 -/
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto c4 = bindings.stream_variable("c4");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1, c2 },
            {},
    });
    auto&& i1 = g0.insert(relation::values {
            { c3, c4 },
            {},
    });

    auto&& r0 = g0.insert(relation::intermediate::intersection {
            {
                    { c0, c3 },
            },
    });
    i0.output() >> r0.left();
    i1.output() >> r0.right();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c1, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c1m = q0.mappings()[0].source();
    EXPECT_NE(c1m, c1);

    ASSERT_EQ(r0.group_key_pairs().size(), 1);
    auto c0m = r0.group_key_pairs()[0].left();
    auto c3m = r0.group_key_pairs()[0].right();
    EXPECT_NE(c0m, c0);
    EXPECT_NE(c3m, c3);

    ASSERT_EQ(i0.columns().size(), 2);
    EXPECT_EQ(i0.columns()[0], c0m);
    EXPECT_EQ(i0.columns()[1], c1m);

    ASSERT_EQ(i1.columns().size(), 1);
    EXPECT_EQ(i1.columns()[0], c3m);
}

TEST_F(rewrite_stream_variables_subquery_test, difference) {
    /*
     * values:i0 -\
     *             +-- difference:r0 -*
     * values:i1 -/
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto c4 = bindings.stream_variable("c4");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1, c2 },
            {},
    });
    auto&& i1 = g0.insert(relation::values {
            { c3, c4 },
            {},
    });

    auto&& r0 = g0.insert(relation::intermediate::difference {
            {
                    { c0, c3 },
            },
    });
    i0.output() >> r0.left();
    i1.output() >> r0.right();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c1, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c1m = q0.mappings()[0].source();
    EXPECT_NE(c1m, c1);

    ASSERT_EQ(r0.group_key_pairs().size(), 1);
    auto c0m = r0.group_key_pairs()[0].left();
    auto c3m = r0.group_key_pairs()[0].right();
    EXPECT_NE(c0m, c0);
    EXPECT_NE(c3m, c3);

    ASSERT_EQ(i0.columns().size(), 2);
    EXPECT_EQ(i0.columns()[0], c0m);
    EXPECT_EQ(i0.columns()[1], c1m);

    ASSERT_EQ(i1.columns().size(), 1);
    EXPECT_EQ(i1.columns()[0], c3m);
}

TEST_F(rewrite_stream_variables_subquery_test, escape) {
    /*
     * values:i0 -- escape:r0 -*
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& i0 = g0.insert(relation::values {
            { c0, c1 },
            {},
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = g0.insert(relation::intermediate::escape {
            {
                    { c0, c2 },
            }
    });
    i0.output() >> r0.input();

    auto s0 = bindings.stream_variable("s0");
    extension::relation::subquery q0 {
            std::move(g0),
            {
                    { c2, s0 },
            },
    };

    rewrite_stream_variables(q0);

    ASSERT_EQ(q0.mappings().size(), 1);
    auto c2m = q0.mappings()[0].source();
    EXPECT_NE(c2m, c2);

    ASSERT_EQ(r0.mappings().size(), 1);
    auto c0m = r0.mappings()[0].source();
    EXPECT_NE(c0m, c0);
    EXPECT_EQ(r0.mappings()[0].destination(), c2m);

    ASSERT_EQ(i0.columns().size(), 1);
    EXPECT_EQ(i0.columns()[0], c0m);
}

TEST_F(rewrite_stream_variables_subquery_test, subquery) {
    /*
     * g0: values:r0 -*
     */
    relation::graph_type g0;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /*
     * q1: subquery:q0 -*
     */
    relation::graph_type g1;
    auto s0 = bindings.stream_variable("s0");
    auto&& q0 = g1.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { c0, s0 },
            },
    });

    auto s1 = bindings.stream_variable("s1");
    extension::relation::subquery q1 {
            std::move(g1),
            {
                    { s0, s1 },
            },
    };

    rewrite_stream_variables(q1);

    ASSERT_EQ(q1.mappings().size(), 1);
    auto s0m = q1.mappings()[0].source();
    EXPECT_NE(s0m, s0);
    EXPECT_EQ(q1.mappings()[0].destination(), s1);

    ASSERT_EQ(q0.mappings().size(), 1);
    EXPECT_EQ(q0.mappings()[0].source(), c0);
    EXPECT_EQ(q0.mappings()[0].destination(), s0m);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0], c0);
}

} // namespace yugawara::analyzer::details
