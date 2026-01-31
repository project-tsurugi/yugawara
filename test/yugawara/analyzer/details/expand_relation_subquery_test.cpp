#include <yugawara/analyzer/details/expand_relation_subquery.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/join.h>

#include <yugawara/binding/factory.h>

#include <yugawara/extension/relation/subquery.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::util::downcast;
using ::takatori::util::clone_unique;

class expand_relation_subquery_test : public ::testing::Test {
protected:
    binding::factory bindings;

    scalar::variable_reference wrap(descriptor::variable variable) {
        return scalar::variable_reference { std::move(variable) };
    }

    descriptor::variable unwrap(scalar::expression const& expr) {
        if (expr.kind() == scalar::variable_reference::tag) {
            auto&& varref = downcast<scalar::variable_reference>(expr);
            return varref.variable();
        }
        throw std::domain_error("expected variable_reference");
    }
};

TEST_F(expand_relation_subquery_test, simple) {
    /* inner query: g0
     *   values:g0r0 -*
     */
    relation::graph_type g0 {};
    auto g0c0 = bindings.stream_variable("g0c0");
    auto&& g0r0 = g0.insert(relation::values {
            {
                    g0c0,
            },
            {},
    });

    /* outer query: graph
     *   subquery:r0 -- emit:r1
     */
    relation::graph_type graph {};
    auto r0o0 = bindings.stream_variable("r0o0");
    auto&& r0 = graph.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { g0c0, r0o0 },
            },
    });
    auto&& r1 = graph.insert(relation::emit {
            { r0o0, "o0" },
    });
    r0.output() >> r1.input();

    expand_relation_subquery(graph);

    // values - project - emit
    ASSERT_EQ(graph.size(), 3);
    ASSERT_TRUE(graph.contains(g0r0));
    ASSERT_TRUE(graph.contains(r1));
    auto&& values = g0r0;
    auto&& project = next<relation::project>(g0r0.output());
    auto&& emit = r1;

    ASSERT_EQ(values.columns().size(), 1);
    auto&& g0c0m = values.columns()[0];
    EXPECT_NE(g0c0m, g0c0);

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), wrap(g0c0m));
    EXPECT_EQ(project.columns()[0].variable(), r0o0);

    ASSERT_EQ(emit.columns().size(), 1);
    EXPECT_EQ(emit.columns()[0].source(), r0o0);
}

TEST_F(expand_relation_subquery_test, complex) {
    /* inner query: g0
     *   values:g0r0 -- filter:g0r1 -- project:g0r2 -*
     */
    relation::graph_type g0 {};
    auto g0c0 = bindings.stream_variable("g0c0");
    auto&& g0r0 = g0.insert(relation::values {
            {
                    g0c0,
            },
            {},
    });
    auto&& g0r1 = g0.insert(relation::filter {
            compare(scalar::variable_reference { g0c0 }, constant(10))
    });
    auto g0c1 = bindings.stream_variable("g0c1");
    auto&& g0r2 = g0.insert(relation::project {
            relation::project::column {
                    g0c1,
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { g0c0 },
                            constant(1),
                    },
            },
    });
    g0r0.output() >> g0r1.input();
    g0r1.output() >> g0r2.input();

    /* outer query: graph
     *   subquery:r0 -- emit:r1
     */
    relation::graph_type graph {};
    auto r0o0 = bindings.stream_variable("r0o0");
    auto&& r0 = graph.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { g0c1, r0o0 },
            },
    });
    auto&& r1 = graph.insert(relation::emit {
            { r0o0, "o0" },
    });
    r0.output() >> r1.input();

    expand_relation_subquery(graph);

    // values - filter - project - escape - emit
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(g0r0));
    ASSERT_TRUE(graph.contains(g0r1));
    ASSERT_TRUE(graph.contains(g0r2));
    ASSERT_TRUE(graph.contains(r1));
    auto&& values = g0r0;
    auto&& filter = g0r1;
    auto&& project = g0r2;
    auto&& escape = next<relation::project>(g0r2.output());
    auto&& emit = r1;

    ASSERT_EQ(values.columns().size(), 1);
    auto&& g0c0m = values.columns()[0];
    EXPECT_NE(g0c0m, g0c0);

    EXPECT_EQ(filter.condition(), compare(scalar::variable_reference { g0c0m }, constant(10)));

    ASSERT_EQ(project.columns().size(), 1);
    auto&& g0c1m = project.columns()[0].variable();
    EXPECT_NE(g0c1m, g0c1);
    EXPECT_EQ(project.columns()[0].value(), (scalar::binary {
            scalar::binary_operator::add,
            scalar::variable_reference { g0c0m },
            constant(1),
    }));

    ASSERT_EQ(escape.columns().size(), 1);
    EXPECT_EQ(escape.columns()[0].value(), wrap(g0c1m));
    EXPECT_EQ(escape.columns()[0].variable(), r0o0);

    ASSERT_EQ(emit.columns().size(), 1);
    EXPECT_EQ(emit.columns()[0].source(), r0o0);
}

TEST_F(expand_relation_subquery_test, multiple_subqueries) {
    /* inner query: g0
     *   values:g0r0 -*
     */
    relation::graph_type g0 {};
    auto g0c0 = bindings.stream_variable("g0c0");
    auto&& g0r0 = g0.insert(relation::values {
            {
                    g0c0,
            },
            {},
    });

    /* inner query: g1
     *   values:g1r0 -*
     */
    relation::graph_type g1 {};
    auto g1c0 = bindings.stream_variable("g1c0");
    auto&& g1r0 = g1.insert(relation::values {
            {
                    g1c0,
            },
            {},
    });

    /* inner query: g2
     *   values:g2r0 -*
     */
    relation::graph_type g2 {};
    auto g2c0 = bindings.stream_variable("g2c0");
    auto&& g2r0 = g2.insert(relation::values {
            {
                    g2c0,
            },
            {},
    });

    /* outer query: graph
     *   subquery:r0 -\
     *                 join:r3 -\
     *   subquery:r1 -/          join:r4 -- emit:r5
     *                          /
     *   subquery:r2 ----------/
     */
    relation::graph_type graph {};
    auto r0o0 = bindings.stream_variable("r0o0");
    auto&& r0 = graph.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { g0c0, r0o0 },
            },
    });
    auto r1o0 = bindings.stream_variable("r1o0");
    auto&& r1 = graph.insert(extension::relation::subquery {
            std::move(g1),
            {
                    { g1c0, r1o0 },
            },
    });
    auto r2o0 = bindings.stream_variable("r2o0");
    auto&& r2 = graph.insert(extension::relation::subquery {
            std::move(g2),
            {
                    { g2c0, r2o0 },
            },
    });
    auto&& r3 = graph.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& r4 = graph.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& r5 = graph.insert(relation::emit {
            { r0o0, "r0" },
            { r1o0, "r1" },
            { r2o0, "r2" },
    });
    r0.output() >> r3.left();
    r1.output() >> r3.right();
    r3.output() >> r4.left();
    r2.output() >> r4.right();
    r4.output() >> r5.input();

    expand_relation_subquery(graph);

    /* values:v0 - project:e0 -\
     *                          join:j0 -\
     * values:v1 - project:e1 -/          join:j1 - emit:e2
     *                                   /
     * values:v2 - project:e2 ----------/
     */
    ASSERT_EQ(graph.size(), 9);
    ASSERT_TRUE(graph.contains(g0r0));
    ASSERT_TRUE(graph.contains(g1r0));
    ASSERT_TRUE(graph.contains(g2r0));
    ASSERT_TRUE(graph.contains(r3));
    ASSERT_TRUE(graph.contains(r4));
    ASSERT_TRUE(graph.contains(r5));
    auto&& v0 = g0r0;
    auto&& v1 = g1r0;
    auto&& v2 = g2r0;
    auto&& e0 = next<relation::project>(v0.output());
    auto&& e1 = next<relation::project>(v1.output());
    auto&& e2 = next<relation::project>(v2.output());
    auto&& j0 = r3;
    auto&& j1 = r4;
    auto&& emit = r5;
    EXPECT_TRUE(e0.output() > j0.left());
    EXPECT_TRUE(e1.output() > j0.right());
    EXPECT_TRUE(e2.output() > j1.right());

    ASSERT_EQ(v0.columns().size(), 1);
    auto&& g0c0m = v0.columns()[0];
    EXPECT_NE(g0c0m, g0c0);

    ASSERT_EQ(v1.columns().size(), 1);
    auto&& g1c0m = v1.columns()[0];
    EXPECT_NE(g1c0m, g1c0);

    ASSERT_EQ(v2.columns().size(), 1);
    auto&& g2c0m = v2.columns()[0];
    EXPECT_NE(g2c0m, g2c0);

    ASSERT_EQ(e0.columns().size(), 1);
    EXPECT_EQ(e0.columns()[0].value(), wrap(g0c0m));
    EXPECT_EQ(e0.columns()[0].variable(), r0o0);

    ASSERT_EQ(e1.columns().size(), 1);
    EXPECT_EQ(e1.columns()[0].value(), wrap(g1c0m));
    EXPECT_EQ(e1.columns()[0].variable(), r1o0);

    ASSERT_EQ(e2.columns().size(), 1);
    EXPECT_EQ(e2.columns()[0].value(), wrap(g2c0m));
    EXPECT_EQ(e2.columns()[0].variable(), r2o0);

    ASSERT_EQ(emit.columns().size(), 3);
    EXPECT_EQ(emit.columns()[0].source(), r0o0);
    EXPECT_EQ(emit.columns()[1].source(), r1o0);
    EXPECT_EQ(emit.columns()[2].source(), r2o0);
}

TEST_F(expand_relation_subquery_test, nesting) {
    /* inner query: g0
     *   values:g0r0 -*
     */
    relation::graph_type g0 {};
    auto g0c0 = bindings.stream_variable("g0c0");
    auto&& g0r0 = g0.insert(relation::values {
            {
                    g0c0,
            },
            {},
    });

    /* inner query: g1
     *  subquery:g1r0 -*
     */
    relation::graph_type g1 {};
    auto g1c0 = bindings.stream_variable("g1c0");
    auto&& g1r0 = g1.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { g0c0, g1c0 },
            },
    });
    (void) g1r0;

    /* inner query: g2
     *  subquery:g2r0 -*
     */
    relation::graph_type g2 {};
    auto g2c0 = bindings.stream_variable("g2c0");
    auto&& g2r0 = g2.insert(extension::relation::subquery {
            std::move(g1),
            {
                    { g1c0, g2c0 },
            },
    });
    (void) g2r0;

    /* outer query: graph
     *   subquery:r0 -- emit:r1
     */
    relation::graph_type graph {};
    auto r0o0 = bindings.stream_variable("r0o0");
    auto&& r0 = graph.insert(extension::relation::subquery {
            std::move(g2),
            {
                    { g2c0, r0o0 },
            },
    });
    auto&& r1 = graph.insert(relation::emit {
            { r0o0, "o0" },
    });
    r0.output() >> r1.input();

    expand_relation_subquery(graph);

    // values - project:e0 - project:e1 - project:e2 - emit
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(g0r0));
    ASSERT_TRUE(graph.contains(r1));
    auto&& values = g0r0;
    auto&& e0 = next<relation::project>(g0r0.output());
    auto&& e1 = next<relation::project>(e0.output());
    auto&& e2 = next<relation::project>(e1.output());
    auto&& emit = r1;
    EXPECT_TRUE(e2.output() > emit.input());

    ASSERT_EQ(values.columns().size(), 1);
    auto&& g0c0m = values.columns()[0];
    EXPECT_NE(g0c0m, g0c0);

    ASSERT_EQ(e0.columns().size(), 1);
    EXPECT_EQ(e0.columns()[0].value(), wrap(g0c0m));
    auto g1c0m = e0.columns()[0].variable();
    EXPECT_NE(g1c0m, g1c0);

    ASSERT_EQ(e1.columns().size(), 1);
    EXPECT_EQ(e1.columns()[0].value(), wrap(g1c0m));
    auto g2c0m = e1.columns()[0].variable();
    EXPECT_NE(g2c0m, g2c0);

    ASSERT_EQ(e2.columns().size(), 1);
    EXPECT_EQ(e2.columns()[0].value(), wrap(g2c0m));
    EXPECT_EQ(e2.columns()[0].variable(), r0o0);

    ASSERT_EQ(emit.columns().size(), 1);
    EXPECT_EQ(emit.columns()[0].source(), r0o0);
}

TEST_F(expand_relation_subquery_test, self_join) {
    /* inner query: g0
     *   values:g0r0 -*
     */
    relation::graph_type g0 {};
    auto g0c0 = bindings.stream_variable("g0c0");
    auto&& g0r0 = g0.insert(relation::values {
            {
                    g0c0,
            },
            {},
    });

    /* outer query: graph
     *   subquery:r0 -\
     *                 join:r2 -- emit:r3
     *   subquery:r1 -/
     */
    relation::graph_type graph {};
    auto r0o0 = bindings.stream_variable("r0o0");
    auto r0o1 = bindings.stream_variable("r0o0");
    auto&& r0 = graph.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { g0c0, r0o0 },
            },
    });
    auto r1o0 = bindings.stream_variable("r1o0");
    auto&& r1 = graph.insert(clone_unique(r0)); // copy of r0
    r1.mappings()[0].destination() = r1o0;
    auto&& r2 = graph.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& r3 = graph.insert(relation::emit {
            { r0o0, "r0" },
            { r1o0, "r1" },
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    expand_relation_subquery(graph);

    /*
     * values:v0 -- emit:e0 -\
     *                        join -- emit
     * values:v1 -- emit:e1 -/
     */
    ASSERT_EQ(graph.size(), 6);
    ASSERT_TRUE(graph.contains(g0r0));
    ASSERT_TRUE(graph.contains(r2));
    ASSERT_TRUE(graph.contains(r3));
    auto&& v0 = g0r0;
    auto&& e0 = next<relation::project>(g0r0.output());
    auto&& join = r2;
    auto&& e1 = next<relation::project>(join.right());
    auto&& v1 = next<relation::values>(e1.input());
    auto&& emit = r3;

    ASSERT_EQ(v0.columns().size(), 1);
    auto&& g0c0m = v0.columns()[0];
    EXPECT_NE(g0c0m, g0c0);

    ASSERT_EQ(v1.columns().size(), 1);
    auto&& g1c0m = v1.columns()[0];
    EXPECT_NE(g1c0m, g0c0);
    EXPECT_NE(g1c0m, g0c0m);

    ASSERT_EQ(e0.columns().size(), 1);
    EXPECT_EQ(e0.columns()[0].value(), wrap(g0c0m));
    EXPECT_EQ(e0.columns()[0].variable(), r0o0);

    ASSERT_EQ(e1.columns().size(), 1);
    EXPECT_EQ(e1.columns()[0].value(), wrap(g1c0m));
    EXPECT_EQ(e1.columns()[0].variable(), r1o0);

    ASSERT_EQ(emit.columns().size(), 2);
    EXPECT_EQ(emit.columns()[0].source(), r0o0);
    EXPECT_EQ(emit.columns()[1].source(), r1o0);
}

} // namespace yugawara::analyzer::details
