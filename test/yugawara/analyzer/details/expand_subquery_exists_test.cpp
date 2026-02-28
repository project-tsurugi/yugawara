#include <yugawara/analyzer/details/expand_subquery.h>

#include <gtest/gtest.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/unary.h>
#include <takatori/scalar/binary.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/util/vector_print_support.h>

#include <yugawara/binding/factory.h>

#include <yugawara/extension/scalar/exists.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::util::downcast;
using ::takatori::util::clone_unique;
using ::takatori::util::print_support;

class expand_subquery_exists_test : public ::testing::Test {
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

    using code_type = intermediate_plan_normalizer_code;
    using diagnostic_type = diagnostic<code_type>;

    bool contains(std::vector<diagnostic_type> const& diagnostics, code_type code) {
        return std::any_of(diagnostics.begin(), diagnostics.end(), [=](diagnostic_type const& diagnostic) {
            return diagnostic.code() == code;
        });
    }
};

TEST_F(expand_subquery_exists_test, filter) {
    /* inner query: g0
     *   values:sv -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* outer query: graph
     *   values:rv -- filter[g0]:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto&& r0 = graph.insert(relation::filter {
            extension::scalar::exists { std::move(g0) },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
    });
    rv.output() >> r0.input();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[->c1] --\
     *                    join -- filter[T]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c1);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::semi);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(r0.condition(), boolean(true));

    ASSERT_EQ(re.columns().size(), 1);
    EXPECT_EQ(re.columns()[0].source(), c1);
}

TEST_F(expand_subquery_exists_test, value) {
    /* inner query: g0
     *   values:sv -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* outer query: graph
     *   values:rv -- project[g0->c2]:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = graph.insert(relation::project {
            relation::project::column {
                    extension::scalar::exists { std::move(g0) },
                    c2,
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
    });
    rv.output() >> r0.input();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[->c1] -----------------------------\
     *                                               join -- project[x IS T->c2]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] -- project[T->x0] -- limit --/
     *
     */
    ASSERT_EQ(graph.size(), 7);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(rv.output(), join.left());
    auto&& limit = next<relation::intermediate::limit>(join.right());
    auto&& project = next<relation::project>(limit.input());
    EXPECT_GT(sv.output(), project.input());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c1);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::left_outer);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), boolean(true));
    auto&& x = project.columns()[0].variable();

    EXPECT_EQ(limit.count(), 1);
    ASSERT_EQ(limit.group_keys().size(), 0);
    ASSERT_EQ(limit.sort_keys().size(), 0);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::left_outer);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].value(), (scalar::unary {
            scalar::unary_operator::is_true,
            wrap(x),
    }));
    EXPECT_EQ(r0.columns()[0].variable(), c2);

    ASSERT_EQ(re.columns().size(), 2);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
}

TEST_F(expand_subquery_exists_test, filter_and) {
    /* inner query: g0
     *   values:sv -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* outer query: graph
     *   values:rv -- filter[g0 & c1>0]:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto&& r0 = graph.insert(relation::filter {
            land(
                    extension::scalar::exists { std::move(g0) },
                    compare(wrap(c1), constant(), scalar::comparison_operator::greater))
    });
    auto&& re = graph.insert(relation::emit {
            c1,
    });
    rv.output() >> r0.input();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[->c1] --\
     *                    join -- filter[T & c1>0]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c1);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::semi);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(r0.condition(), land(
            boolean(true),
            compare(wrap(c1), constant(), scalar::comparison_operator::greater)));

    ASSERT_EQ(re.columns().size(), 1);
    EXPECT_EQ(re.columns()[0].source(), c1);
}


TEST_F(expand_subquery_exists_test, filter_or) {
    /* inner query: g0
     *   values:sv -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* outer query: graph
     *   values:rv -- filter[g0 | c1>0]:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto&& r0 = graph.insert(relation::filter {
            lor(
                    extension::scalar::exists { std::move(g0) },
                    compare(wrap(c1), constant(), scalar::comparison_operator::greater))
    });
    auto&& re = graph.insert(relation::emit {
            c1,
    });
    rv.output() >> r0.input();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[->c1] -----------------------------\
     *                                               join -- filter[x IS T | c1>0]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] -- project[T->x0] -- limit --/
     *
     */
    ASSERT_EQ(graph.size(), 7);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(rv.output(), join.left());
    auto&& limit = next<relation::intermediate::limit>(join.right());
    auto&& project = next<relation::project>(limit.input());
    EXPECT_GT(sv.output(), project.input());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c1);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::left_outer);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), boolean(true));
    auto&& x = project.columns()[0].variable();

    EXPECT_EQ(limit.count(), 1);
    ASSERT_EQ(limit.group_keys().size(), 0);
    ASSERT_EQ(limit.sort_keys().size(), 0);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::left_outer);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(r0.condition(), lor(
            scalar::unary {
                    scalar::unary_operator::is_true,
                    wrap(x),
            },
            compare(wrap(c1), constant(), scalar::comparison_operator::greater)));

    ASSERT_EQ(re.columns().size(), 1);
    EXPECT_EQ(re.columns()[0].source(), c1);
}


TEST_F(expand_subquery_exists_test, filter_not) {
    /* inner query: g0
     *   values:sv -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* outer query: graph
     *   values:rv -- filter[!g0]:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto&& r0 = graph.insert(relation::filter {
            scalar::unary {
                    scalar::unary_operator::conditional_not,
                    extension::scalar::exists { std::move(g0) },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
    });
    rv.output() >> r0.input();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[->c1] --\
     *                    join -- filter[T]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c1);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::anti);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(r0.condition(), boolean(true));

    ASSERT_EQ(re.columns().size(), 1);
    EXPECT_EQ(re.columns()[0].source(), c1);
}

TEST_F(expand_subquery_exists_test, value_not) {
    /* inner query: g0
     *   values:sv -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* outer query: graph
     *   values:rv -- project[!g0->c2]:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = graph.insert(relation::project {
            relation::project::column {
                    scalar::unary {
                            scalar::unary_operator::conditional_not,
                            extension::scalar::exists { std::move(g0) },
                    },
                    c2,
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
    });
    rv.output() >> r0.input();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[->c1] -----------------------------\
     *                                               join -- project[x IS N->c2]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] -- project[T->x0] -- limit --/
     *
     */
    ASSERT_EQ(graph.size(), 7);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(rv.output(), join.left());
    auto&& limit = next<relation::intermediate::limit>(join.right());
    auto&& project = next<relation::project>(limit.input());
    EXPECT_GT(sv.output(), project.input());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c1);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::left_outer);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), boolean(true));
    auto&& x = project.columns()[0].variable();

    EXPECT_EQ(limit.count(), 1);
    ASSERT_EQ(limit.group_keys().size(), 0);
    ASSERT_EQ(limit.sort_keys().size(), 0);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::left_outer);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].value(), (scalar::unary {
            scalar::unary_operator::is_null,
            wrap(x),
    }));
    EXPECT_EQ(r0.columns()[0].variable(), c2);

    ASSERT_EQ(re.columns().size(), 2);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
}

TEST_F(expand_subquery_exists_test, join_condition) {
    /* inner query: g0
     *   values:sv -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* outer query: graph
     *   values:rv0 --\
     *                 join[g] -- emit:re
     *   values:rv1 --/
     */
    relation::graph_type graph {};
    auto c2 = bindings.stream_variable("c2");
    auto&& rv1 = graph.insert(relation::values {
            {
                    c2,
            },
            {},
    });
    auto c3 = bindings.stream_variable("c3");
    auto&& rv2 = graph.insert(relation::values {
            {
                    c3,
            },
            {},
    });
    auto&& r0 = graph.insert(relation::intermediate::join {
            // INNER JOIN,
             relation::join_kind::inner,
             // no other conditions
            extension::scalar::exists { std::move(g0) },
    });
    auto&& re = graph.insert(relation::emit {
            c2,
            c3,
    });
    rv1.output() >> r0.left();
    rv2.output() >> r0.right();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

} // namespace yugawara::analyzer::details
