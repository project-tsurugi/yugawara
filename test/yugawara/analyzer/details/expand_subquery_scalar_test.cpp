#include <yugawara/analyzer/details/expand_subquery.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/scalar/unary.h>
#include <takatori/scalar/cast.h>
#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>
#include <takatori/scalar/match.h>
#include <takatori/scalar/conditional.h>
#include <takatori/scalar/coalesce.h>
#include <takatori/scalar/let.h>
#include <takatori/scalar/function_call.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/union.h>

#include <takatori/util/vector_print_support.h>

#include <yugawara/binding/factory.h>

#include <yugawara/function/declaration.h>

#include <yugawara/storage/configurable_provider.h>

#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/relation/subquery.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::util::downcast;
using ::takatori::util::clone_unique;
using ::takatori::util::print_support;

class expand_subquery_scalar_test : public ::testing::Test {
protected:
    binding::factory bindings;
    storage::configurable_provider storages;
    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int8() },
                    { "C1", t::int8() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];

    std::shared_ptr<storage::index> i0 = storages.add_index({
            t0,
            "I0",
    });

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

    template<class TExpr>
    [[nodiscard]] std::unique_ptr<scalar::expression> build_scalar(
            descriptor::variable output_column,
            std::function<TExpr(extension::scalar::subquery&&)> const& builder) {
        /* inner query: g0
         *   values:sv -*
         */
        relation::graph_type g0 {};
        auto c0 = std::move(output_column);
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
                        builder(extension::scalar::subquery { std::move(g0), c0 }),
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
        EXPECT_TRUE(diagnostics.empty()) << print_support(diagnostics);
        if (HasFailure()) {
            return {};
        }

        /*
         * values:rv[->c1] --\
         *                    join -- project[c0->c2]:r0 -- emit[c1, c2]:re
         * values:sv[->c0] --/
         *
         */
        EXPECT_EQ(graph.size(), 5);
        EXPECT_TRUE(graph.contains(sv));
        EXPECT_TRUE(graph.contains(rv));
        EXPECT_TRUE(graph.contains(r0));
        EXPECT_TRUE(graph.contains(re));
        if (HasFailure()) {
            return {};
        }
        auto&& join = next<relation::intermediate::join>(rv.output());
        EXPECT_GT(rv.output(), join.left());
        EXPECT_GT(sv.output(), join.right());
        if (r0.columns().size() != 1) {
            ADD_FAILURE() << "unexpected number of columns in project:r0: " << r0.columns().size();
            return {};
        }
        if (r0.columns()[0].variable() != c2) {
            ADD_FAILURE() << "unexpected variable in project:r0: " << r0.columns()[0].variable() << " (expected: " << c2 << ")";
            return {};
        }
        return r0.columns()[0].release_value();
    }
};

TEST_F(expand_subquery_scalar_test, simple) {
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
                    extension::scalar::subquery { std::move(g0), c0 },
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
     * values:rv[->c1] --\
     *                    join -- project[c0->c2]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     *
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c1);

    EXPECT_EQ(join.operator_kind(), relation::join_kind::left_outer_at_most_one);
    EXPECT_FALSE(join.condition());

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].value(), wrap(c0));
    EXPECT_EQ(r0.columns()[0].variable(), c2);

    ASSERT_EQ(re.columns().size(), 2);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
}

TEST_F(expand_subquery_scalar_test, relation_find) {
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
     *   find:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c1");
    auto&& r0 = graph.insert(relation::find {
            bindings(i0),
            {
                    { bindings(t0c0), c1 },
            },
            {
                    relation::find::key {
                            bindings(t0c0),
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
    });
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_scan_lower) {
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
     *   scan:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c1");
    auto&& r0 = graph.insert(relation::scan {
            bindings(i0),
            {
                    { bindings(t0c0), c1 },
            },
            {
                    relation::scan::key {
                            bindings(t0c0),
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
            {},
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
    });
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_scan_upper) {
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
     *   scan:r0 -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c1");
    auto&& r0 = graph.insert(relation::scan {
            bindings(i0),
            {
                    { bindings(t0c0), c1 },
            },
            {},
            {
                    relation::scan::key {
                            bindings(t0c0),
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
    });
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_join_find_key) {
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
     *   values:rv -- join_find:r0 -- emit:re
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
    auto&& r0 = graph.insert(relation::join_find {
            ::takatori::relation::join_kind::inner,
            bindings(i0),
            {
                    { bindings(t0c0), c2 },
            },
            {
                    relation::join_find::key {
                            bindings(t0c0),
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
            },
            {},
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c0,
    });
    rv.output() >> r0.left();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_join_find_condition) {
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
     *   values:rv -- join_find:r0 -- emit:re
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
    auto&& r0 = graph.insert(relation::join_find {
            ::takatori::relation::join_kind::inner,
            bindings(i0),
            {
                    { bindings(t0c0), c2 },
            },
            {
                    relation::join_find::key {
                            bindings(t0c0),
                            constant(),
                    },
            },
            {
                    compare(
                            constant(),
                            extension::scalar::subquery { std::move(g0), c0 }
                    ),
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c0,
    });
    rv.output() >> r0.left();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_join_scan_lower) {
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
     *   values:rv -- join_find:r0 -- emit:re
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
    auto&& r0 = graph.insert(relation::join_scan {
            ::takatori::relation::join_kind::inner,
            bindings(i0),
            {
                    { bindings(t0c0), c2 },
            },
            {
                    relation::join_scan::key {
                            bindings(t0c0),
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
            {},
            {},
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c0,
    });
    rv.output() >> r0.left();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_join_scan_upper) {
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
     *   values:rv -- join_find:r0 -- emit:re
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
    auto&& r0 = graph.insert(relation::join_scan {
            ::takatori::relation::join_kind::inner,
            bindings(i0),
            {
                    { bindings(t0c0), c2 },
            },
            {},
            {
                    relation::join_scan::key {
                            bindings(t0c0),
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
            {},
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c0,
    });
    rv.output() >> r0.left();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_join_scan_condition) {
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
     *   values:rv -- join_find:r0 -- emit:re
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
    auto&& r0 = graph.insert(relation::join_scan {
            ::takatori::relation::join_kind::inner,
            bindings(i0),
            {
                    { bindings(t0c0), c2 },
            },
            {},
            {
                    relation::join_scan::key {
                            bindings(t0c0),
                            constant(),
                    },
                    relation::endpoint_kind::inclusive,
            },
            {
                    compare(
                            constant(),
                            extension::scalar::subquery { std::move(g0), c0 }
                    ),
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c0,
    });
    rv.output() >> r0.left();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_apply) {
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
     *   values:rv -- apply[g0->c2]:r0 -- emit:re
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
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "x0", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r0 = graph.insert(relation::apply {
            tvf,
            {
                    extension::scalar::subquery { std::move(g0), c0 },
            },
            {
                    { 0, c2, }
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
     * values:rv[->c1] --\
     *                    join -- apply[c0->c2]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     *
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    ASSERT_EQ(r0.arguments().size(), 1);
    EXPECT_EQ(r0.arguments()[0], wrap(c0));
}

TEST_F(expand_subquery_scalar_test, relation_project) {
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
                    extension::scalar::subquery { std::move(g0), c0 },
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
     * values:rv[->c1] --\
     *                    join -- project[c0->c2]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     *
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].value(), wrap(c0));
    EXPECT_EQ(r0.columns()[0].variable(), c2);
}

TEST_F(expand_subquery_scalar_test, relation_project_multiple_columns) {
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
    auto c3 = bindings.stream_variable("c3");
    auto c4 = bindings.stream_variable("c4");
    auto&& r0 = graph.insert(relation::project {
            relation::project::column {
                    constant(),
                    c2,
            },
            relation::project::column {
                    extension::scalar::subquery { std::move(g0), c0 },
                    c3,
            },
            relation::project::column {
                    constant(),
                    c4,
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c3,
            c4,
    });
    rv.output() >> r0.input();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[->c1] --\
     *                    join -- project[c0->c2]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0].value(), constant());
    EXPECT_EQ(r0.columns()[0].variable(), c2);
    EXPECT_EQ(r0.columns()[1].value(), wrap(c0));
    EXPECT_EQ(r0.columns()[1].variable(), c3);
    EXPECT_EQ(r0.columns()[2].value(), constant());
    EXPECT_EQ(r0.columns()[2].variable(), c4);
}

TEST_F(expand_subquery_scalar_test, relation_filter) {
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
            compare(
                    extension::scalar::subquery { std::move(g0), c0 },
                    constant()
            ),
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
     *                    join -- filter:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());

    EXPECT_EQ(r0.condition(), compare(wrap(c0), constant()));
}

TEST_F(expand_subquery_scalar_test, values_simple) {
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
     *   values[g0->c1]:rv -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {
                    {
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
    });
    rv.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[] ------\
     *                    join -- project[c0->c1]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());
    auto&& project = next<relation::project>(join.output());
    EXPECT_GT(project.output(), re.input());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 0);
    ASSERT_EQ(rv.rows().size(), 1);
    ASSERT_EQ(rv.rows()[0].elements().size(), 0);

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), wrap(c0));
    EXPECT_EQ(project.columns()[0].variable(), c1);

    ASSERT_EQ(re.columns().size(), 1);
    EXPECT_EQ(re.columns()[0].source(), c1);
}

TEST_F(expand_subquery_scalar_test, values_single_row) {
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
     *   values[g0->c1]:rv -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
                    c2,
                    c3,
            },
            {
                    {
                            constant(1),
                            extension::scalar::subquery { std::move(g0), c0 },
                            constant(3),
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c3,
    });
    rv.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[] ------\
     *                    join -- project[c0->c2]:r0 -- emit[c1, c2, c3]:re
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());
    auto&& project = next<relation::project>(join.output());
    EXPECT_GT(project.output(), re.input());

    ASSERT_EQ(rv.columns().size(), 2);
    EXPECT_EQ(rv.columns()[0], c1);
    EXPECT_EQ(rv.columns()[1], c3);

    ASSERT_EQ(rv.rows().size(), 1);
    ASSERT_EQ(rv.rows()[0].elements().size(), 2);
    EXPECT_EQ(rv.rows()[0].elements()[0], constant(1));
    EXPECT_EQ(rv.rows()[0].elements()[1], constant(3));

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), wrap(c0));
    EXPECT_EQ(project.columns()[0].variable(), c2);

    ASSERT_EQ(re.columns().size(), 3);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
    EXPECT_EQ(re.columns()[2].source(), c3);
}

TEST_F(expand_subquery_scalar_test, values_multiple_subqueries_in_row) {
    /* inner query: g0
     *   values:sv0 -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv0 = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* inner query: g1
     *   values:sv1 -*
     */
    relation::graph_type g1 {};
    auto c1 = bindings.stream_variable("c1");
    auto&& sv1 = g1.insert(relation::values {
            {
                    c1,
            },
            {},
    });


    /* outer query: graph
     *   values[g0->c1]:rv -- emit:re
     */
    relation::graph_type graph {};
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto c4 = bindings.stream_variable("c4");
    auto&& rv = graph.insert(relation::values {
            {
                    c2,
                    c3,
                    c4,
            },
            {
                    {
                            extension::scalar::subquery { std::move(g0), c0 },
                            constant(2),
                            extension::scalar::subquery { std::move(g1), c1 },
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c2,
            c3,
            c4,
    });
    rv.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[] -------\
     *                     join:j0 --\
     * values:sv0[->c0] --/           join:j1 -- project[c0->c2, c1->c4]:p0 -- emit[c2, c3, c4]:re
     *                               /
     * values:sv1[->c1] ------------/
     */
    ASSERT_EQ(graph.size(), 7);
    ASSERT_TRUE(graph.contains(sv0));
    ASSERT_TRUE(graph.contains(sv1));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(re));
    auto&& p0 = next<relation::project>(re.input());
    auto&& j1 = next<relation::intermediate::join>(p0.input());
    auto&& j0 = next<relation::intermediate::join>(j1.left());
    EXPECT_GT(rv.output(), j0.left());
    EXPECT_GT(sv0.output(), j0.right());
    EXPECT_GT(sv1.output(), j1.right());

    ASSERT_EQ(rv.columns().size(), 1);
    EXPECT_EQ(rv.columns()[0], c3);

    ASSERT_EQ(rv.rows().size(), 1);
    ASSERT_EQ(rv.rows()[0].elements().size(), 1);
    EXPECT_EQ(rv.rows()[0].elements()[0], constant(2));

    ASSERT_EQ(p0.columns().size(), 2);
    EXPECT_EQ(p0.columns()[0].value(), wrap(c0));
    EXPECT_EQ(p0.columns()[0].variable(), c2);
    EXPECT_EQ(p0.columns()[1].value(), wrap(c1));
    EXPECT_EQ(p0.columns()[1].variable(), c4);

    ASSERT_EQ(re.columns().size(), 3);
    EXPECT_EQ(re.columns()[0].source(), c2);
    EXPECT_EQ(re.columns()[1].source(), c3);
    EXPECT_EQ(re.columns()[2].source(), c4);
}

TEST_F(expand_subquery_scalar_test, values_multiple_rows_first) {
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
     *   values[g0->c1]:rv -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    // VALUES (1, (VALUES ()), 3), (4, 5, 6), (7, 8, 9)
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
                    c2,
                    c3,
            },
            {
                    {
                            constant(1),
                            extension::scalar::subquery { std::move(g0), c0 },
                            constant(3),
                    },
                    {
                            constant(4), constant(5), constant(6),
                    },
                    {
                            constant(7), constant(8), constant(9),
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c3,
    });
    rv.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:v0[0:0] ---\
     *                    join:j0 -- project[c0->c2]:p0 --\
     * values:sv[->c0] --/                                 \
     *                                                      union:u0 -- emit[c1, c2, c3]:re
     * values:v1[1:2] -------------------------------------/
     */
    ASSERT_EQ(graph.size(), 7);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(re));
    auto&& u0 = next<relation::intermediate::union_>(re.input());
    auto&& p0 = next<relation::project>(u0.left());
    auto&& j0 = next<relation::intermediate::join>(p0.input());
    auto&& v0 = next<relation::values>(j0.left());
    EXPECT_GT(sv.output(), j0.right());
    auto&& v1 = next<relation::values>(u0.right());

    ASSERT_EQ(v0.columns().size(), 2);
    auto&& v0c1 = v0.columns()[0];
    auto&& v0c3 = v0.columns()[1];
    EXPECT_NE(v0c1, c1);
    EXPECT_NE(v0c3, c3);

    ASSERT_EQ(v0.rows().size(), 1);
    EXPECT_EQ(v0.rows()[0].elements()[0], constant(1));
    EXPECT_EQ(v0.rows()[0].elements()[1], constant(3));

    ASSERT_EQ(v1.columns().size(), 3);
    auto&& v1c1 = v1.columns()[0];
    auto&& v1c2 = v1.columns()[1];
    auto&& v1c3 = v1.columns()[2];
    EXPECT_NE(v1c1, c1);
    EXPECT_NE(v1c2, c2);
    EXPECT_NE(v1c3, c3);

    ASSERT_EQ(v1.rows().size(), 2);
    EXPECT_EQ(v1.rows()[0].elements()[0], constant(4));
    EXPECT_EQ(v1.rows()[0].elements()[1], constant(5));
    EXPECT_EQ(v1.rows()[0].elements()[2], constant(6));
    EXPECT_EQ(v1.rows()[1].elements()[0], constant(7));
    EXPECT_EQ(v1.rows()[1].elements()[1], constant(8));
    EXPECT_EQ(v1.rows()[1].elements()[2], constant(9));

    ASSERT_EQ(p0.columns().size(), 1);
    EXPECT_EQ(p0.columns()[0].value(), wrap(c0));
    auto&& v0c2 = p0.columns()[0].variable();
    EXPECT_NE(v0c2, c2);

    ASSERT_EQ(u0.mappings().size(), 3);
    EXPECT_EQ(u0.mappings()[0].left(), v0c1);
    EXPECT_EQ(u0.mappings()[0].right(), v1c1);
    EXPECT_EQ(u0.mappings()[0].destination(), c1);
    EXPECT_EQ(u0.mappings()[1].left(), v0c2);
    EXPECT_EQ(u0.mappings()[1].right(), v1c2);
    EXPECT_EQ(u0.mappings()[1].destination(), c2);
    EXPECT_EQ(u0.mappings()[2].left(), v0c3);
    EXPECT_EQ(u0.mappings()[2].right(), v1c3);
    EXPECT_EQ(u0.mappings()[2].destination(), c3);

    ASSERT_EQ(re.columns().size(), 3);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
    EXPECT_EQ(re.columns()[2].source(), c3);
}

TEST_F(expand_subquery_scalar_test, values_multiple_rows_last) {
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
     *   values[g0->c1]:rv -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    // VALUES (1, 2, 3), (4, 5, 6), (7, (VALUES ()), 9)
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
                    c2,
                    c3,
            },
            {
                    {
                            constant(1), constant(2), constant(3),
                    },
                    {
                            constant(4), constant(5), constant(6),
                    },
                    {
                            constant(7),
                            extension::scalar::subquery { std::move(g0), c0 },
                            constant(9),
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c3,
    });
    rv.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:v0[0:1] -------------------------------------\
     *                                                      union:u0 -- emit[c1, c2, c3]:re
     * values:v1[2:2] ---\                                 /
     *                    join:j0 -- project[c0->c2]:p0 --/
     * values:sv[->c0] --/
     */
    ASSERT_EQ(graph.size(), 7);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(re));
    auto&& u0 = next<relation::intermediate::union_>(re.input());
    auto&& v0 = next<relation::values>(u0.left());
    auto&& p0 = next<relation::project>(u0.right());
    auto&& j0 = next<relation::intermediate::join>(p0.input());
    auto&& v1 = next<relation::values>(j0.left());
    EXPECT_GT(sv.output(), j0.right());

    ASSERT_EQ(v0.columns().size(), 3);
    auto&& v0c1 = v0.columns()[0];
    auto&& v0c2 = v0.columns()[1];
    auto&& v0c3 = v0.columns()[2];
    EXPECT_NE(v0c1, c1);
    EXPECT_NE(v0c2, c2);
    EXPECT_NE(v0c3, c3);

    ASSERT_EQ(v0.rows().size(), 2);
    EXPECT_EQ(v0.rows()[0].elements()[0], constant(1));
    EXPECT_EQ(v0.rows()[0].elements()[1], constant(2));
    EXPECT_EQ(v0.rows()[0].elements()[2], constant(3));
    EXPECT_EQ(v0.rows()[1].elements()[0], constant(4));
    EXPECT_EQ(v0.rows()[1].elements()[1], constant(5));
    EXPECT_EQ(v0.rows()[1].elements()[2], constant(6));

    ASSERT_EQ(v1.columns().size(), 2);
    auto&& v1c1 = v1.columns()[0];
    auto&& v1c3 = v1.columns()[1];
    EXPECT_NE(v1c1, c1);
    EXPECT_NE(v1c3, c3);

    ASSERT_EQ(v1.rows().size(), 1);
    EXPECT_EQ(v1.rows()[0].elements()[0], constant(7));
    EXPECT_EQ(v1.rows()[0].elements()[1], constant(9));

    ASSERT_EQ(p0.columns().size(), 1);
    EXPECT_EQ(p0.columns()[0].value(), wrap(c0));
    auto&& v1c2 = p0.columns()[0].variable();
    EXPECT_NE(v1c2, c2);

    ASSERT_EQ(u0.mappings().size(), 3);
    EXPECT_EQ(u0.mappings()[0].left(), v0c1);
    EXPECT_EQ(u0.mappings()[0].right(), v1c1);
    EXPECT_EQ(u0.mappings()[0].destination(), c1);
    EXPECT_EQ(u0.mappings()[1].left(), v0c2);
    EXPECT_EQ(u0.mappings()[1].right(), v1c2);
    EXPECT_EQ(u0.mappings()[1].destination(), c2);
    EXPECT_EQ(u0.mappings()[2].left(), v0c3);
    EXPECT_EQ(u0.mappings()[2].right(), v1c3);
    EXPECT_EQ(u0.mappings()[2].destination(), c3);

    ASSERT_EQ(re.columns().size(), 3);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
    EXPECT_EQ(re.columns()[2].source(), c3);
}

TEST_F(expand_subquery_scalar_test, values_multiple_rows_middle) {
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
     *   values[g0->c1]:rv -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    // VALUES (1, 2, 3), (4, (VALUES ()), 6), (7, 8, 9)
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
                    c2,
                    c3,
            },
            {
                    {
                            constant(1), constant(2), constant(3),
                    },
                    {
                            constant(4),
                            extension::scalar::subquery { std::move(g0), c0 },
                            constant(6),
                    },
                    {
                            constant(7), constant(8), constant(9),
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c3,
    });
    rv.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:v0[0:0] -------------------------------------\
     *                                                      union:u0 --\
     * values:v1[1:1] ---\                                 /            \
     *                    join:j0 -- project[c0->c2]:p0 --/              union:u1 -- emit[c1, c2, c3]:re
     * values:sv[->c0] --/                                              /
     *                                                                 /
     * values:v2[2:2] ------------------------------------------------/
     */
    ASSERT_EQ(graph.size(), 9);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(re));
    auto&& u1 = next<relation::intermediate::union_>(re.input());
    auto&& u0 = next<relation::intermediate::union_>(u1.left());
    auto&& v0 = next<relation::values>(u0.left());
    auto&& p0 = next<relation::project>(u0.right());
    auto&& j0 = next<relation::intermediate::join>(p0.input());
    auto&& v1 = next<relation::values>(j0.left());
    EXPECT_GT(sv.output(), j0.right());
    auto&& v2 = next<relation::values>(u1.right());

    ASSERT_EQ(v0.columns().size(), 3);
    auto&& v0c1 = v0.columns()[0];
    auto&& v0c2 = v0.columns()[1];
    auto&& v0c3 = v0.columns()[2];
    EXPECT_NE(v0c1, c1);
    EXPECT_NE(v0c2, c2);
    EXPECT_NE(v0c3, c3);

    ASSERT_EQ(v0.rows().size(), 1);
    EXPECT_EQ(v0.rows()[0].elements()[0], constant(1));
    EXPECT_EQ(v0.rows()[0].elements()[1], constant(2));
    EXPECT_EQ(v0.rows()[0].elements()[2], constant(3));

    ASSERT_EQ(v1.columns().size(), 2);
    auto&& v1c1 = v1.columns()[0];
    auto&& v1c3 = v1.columns()[1];
    EXPECT_NE(v1c1, c1);
    EXPECT_NE(v1c3, c3);

    ASSERT_EQ(v1.rows().size(), 1);
    EXPECT_EQ(v1.rows()[0].elements()[0], constant(4));
    EXPECT_EQ(v1.rows()[0].elements()[1], constant(6));

    ASSERT_EQ(v2.columns().size(), 3);
    auto&& v2c1 = v2.columns()[0];
    auto&& v2c2 = v2.columns()[1];
    auto&& v2c3 = v2.columns()[2];
    EXPECT_NE(v2c1, c1);
    EXPECT_NE(v2c2, c2);
    EXPECT_NE(v2c3, c3);

    ASSERT_EQ(v2.rows().size(), 1);
    EXPECT_EQ(v2.rows()[0].elements()[0], constant(7));
    EXPECT_EQ(v2.rows()[0].elements()[1], constant(8));
    EXPECT_EQ(v2.rows()[0].elements()[2], constant(9));

    ASSERT_EQ(p0.columns().size(), 1);
    EXPECT_EQ(p0.columns()[0].value(), wrap(c0));
    auto&& v1c2 = p0.columns()[0].variable();
    EXPECT_NE(v1c2, c2);

    ASSERT_EQ(u0.mappings().size(), 3);
    EXPECT_EQ(u0.mappings()[0].left(), v0c1);
    EXPECT_EQ(u0.mappings()[0].right(), v1c1);
    auto&& u0c1 = u0.mappings()[0].destination();
    EXPECT_NE(u0c1, c1);
    EXPECT_EQ(u0.mappings()[1].left(), v0c2);
    EXPECT_EQ(u0.mappings()[1].right(), v1c2);
    auto&& u0c2 = u0.mappings()[1].destination();
    EXPECT_NE(u0c2, c2);
    EXPECT_EQ(u0.mappings()[2].left(), v0c3);
    EXPECT_EQ(u0.mappings()[2].right(), v1c3);
    auto&& u0c3 = u0.mappings()[2].destination();
    EXPECT_NE(u0c3, c3);

    ASSERT_EQ(u1.mappings().size(), 3);
    EXPECT_EQ(u1.mappings()[0].left(), u0c1);
    EXPECT_EQ(u1.mappings()[0].right(), v2c1);
    EXPECT_EQ(u1.mappings()[0].destination(), c1);
    EXPECT_EQ(u1.mappings()[1].left(), u0c2);
    EXPECT_EQ(u1.mappings()[1].right(), v2c2);
    EXPECT_EQ(u1.mappings()[1].destination(), c2);
    EXPECT_EQ(u1.mappings()[2].left(), u0c3);
    EXPECT_EQ(u1.mappings()[2].right(), v2c3);
    EXPECT_EQ(u1.mappings()[2].destination(), c3);

    ASSERT_EQ(re.columns().size(), 3);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
    EXPECT_EQ(re.columns()[2].source(), c3);
}

TEST_F(expand_subquery_scalar_test, values_subexpression) {
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
     *   values[(g0+1)->c1]:rv -- emit:re
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv = graph.insert(relation::values {
            {
                    c1,
            },
            {
                    {
                            scalar::binary {
                                    scalar::binary_operator::add,
                                    extension::scalar::subquery { std::move(g0), c0 },
                                    constant(1),
                            },
                    },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c1,
    });
    rv.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:rv[] ------\
     *                    join -- project[(c0+1)->c1]:r0 -- emit[c1, c2]:re
     * values:sv[->c0] --/
     */
    dump(graph, std::cout);
    ASSERT_EQ(graph.size(), 5);
    ASSERT_TRUE(graph.contains(sv));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(re));
    auto&& join = next<relation::intermediate::join>(rv.output());
    EXPECT_GT(rv.output(), join.left());
    EXPECT_GT(sv.output(), join.right());
    auto&& project = next<relation::project>(join.output());
    EXPECT_GT(project.output(), re.input());

    ASSERT_EQ(sv.columns().size(), 1);
    EXPECT_EQ(sv.columns()[0], c0);

    ASSERT_EQ(rv.columns().size(), 0);
    ASSERT_EQ(rv.rows().size(), 1);
    ASSERT_EQ(rv.rows()[0].elements().size(), 0);

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), (scalar::binary {
            scalar::binary_operator::add,
            wrap(c0),
            constant(1),
    }));
    EXPECT_EQ(project.columns()[0].variable(), c1);

    ASSERT_EQ(re.columns().size(), 1);
    EXPECT_EQ(re.columns()[0].source(), c1);
}

TEST_F(expand_subquery_scalar_test, relation_join_group_lower) {
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
     *                 join_group:r0 -- emit:re
     *   values:rv1 --/
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv0 = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& rv1 = graph.insert(relation::values {
            {
                    c2,
            },
            {},
    });
    auto c3 = bindings.stream_variable("c3");
    auto&& r0 = graph.insert(relation::intermediate::join {
            ::takatori::relation::join_kind::inner,
            {
                    relation::intermediate::join::key {
                            c1,
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
            {},
            {},
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c0,
    });
    rv0.output() >> r0.left();
    rv1.output() >> r0.right();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_join_group_upper) {
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
     *                 join_group:r0 -- emit:re
     *   values:rv1 --/
     */
    relation::graph_type graph {};
    auto c1 = bindings.stream_variable("c1");
    auto&& rv0 = graph.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& rv1 = graph.insert(relation::values {
            {
                    c2,
            },
            {},
    });
    auto c3 = bindings.stream_variable("c3");
    auto&& r0 = graph.insert(relation::intermediate::join {
            ::takatori::relation::join_kind::inner,
            {},
            {
                    relation::intermediate::join::key {
                            c2,
                            extension::scalar::subquery { std::move(g0), c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
            {},
    });
    auto&& re = graph.insert(relation::emit {
            c1,
            c2,
            c0,
    });
    rv0.output() >> r0.left();
    rv1.output() >> r0.right();
    r0.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    EXPECT_TRUE(contains(diagnostics, code_type::unsupported_scalar_subquery_placement)) << print_support(diagnostics);
}

TEST_F(expand_subquery_scalar_test, relation_subquery) {
    /* inner query: g0
     *   values:sv0 --*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv0 = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* inner query: g1
     *   values:sv1 -- filter[g0]:r0 --*
     */
    relation::graph_type g1 {};
    auto c1 = bindings.stream_variable("c1");
    auto&& sv1 = g1.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto&& r0 = g1.insert(relation::filter {
            compare(
                    extension::scalar::subquery { std::move(g0), c0 },
                    wrap(c1)
            ),
    });
    sv1.output() >> r0.input();

    /* outer query: graph
     *   subquery[g1]:r1 -- emit:re
     */
    relation::graph_type graph {};
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto&& r1 = graph.insert(extension::relation::subquery {
            std::move(g1),
            {
                    { c0, c2 },
                    { c1, c3 },
            },
    });
    auto&& re = graph.insert(relation::emit {
            c2,
            c3,
    });
    r1.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:sv1[->c1] --\
     *                     join:j0 -- filter:r0 -- project[c0->c2, c1->c3]:p0 -- emit[c2, c3]:re
     * values:sv0[->c0] --/
     */
    ASSERT_EQ(graph.size(), 6);
    ASSERT_TRUE(graph.contains(sv0));
    ASSERT_TRUE(graph.contains(sv1));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& j0 = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(sv1.output(), j0.left());
    EXPECT_GT(sv0.output(), j0.right());
    auto&& p0 = next<relation::project>(re.input());
    EXPECT_GT(r0.output(), p0.input());

    EXPECT_EQ(r0.condition(), compare(wrap(c0), wrap(c1)));

    ASSERT_EQ(p0.columns().size(), 2);
    EXPECT_EQ(p0.columns()[0].value(), wrap(c0));
    EXPECT_EQ(p0.columns()[0].variable(), c2);
    EXPECT_EQ(p0.columns()[1].value(), wrap(c1));
    EXPECT_EQ(p0.columns()[1].variable(), c3);

    ASSERT_EQ(re.columns().size(), 2);
    EXPECT_EQ(re.columns()[0].source(), c2);
    EXPECT_EQ(re.columns()[1].source(), c3);
}

TEST_F(expand_subquery_scalar_test, relation_subquery_clone) {
    /* inner query: g0
     *   values:sv0 --*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv0 = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* inner query: g1
     *   values:sv1 -- filter[g0]:r0 --*
     */
    relation::graph_type g1 {};
    auto c1 = bindings.stream_variable("c1");
    auto&& sv1 = g1.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto&& r0 = g1.insert(relation::filter {
            compare(
                    extension::scalar::subquery { std::move(g0), c0 },
                    wrap(c1)
            ),
    });
    sv1.output() >> r0.input();

    /* outer query: graph
     *   subquery[g1]:r1 -- emit:re
     */
    relation::graph_type graph {};
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto&& r1 = graph.insert(extension::relation::subquery {
            std::move(g1),
            {
                    { c0, c2 },
                    { c1, c3 },
            },
            true, // cloned
    });
    auto&& re = graph.insert(relation::emit {
            c2,
            c3,
    });
    r1.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * values:sv1[->c1] --\
     *                     join:j0 -- filter:r0 -- project[c0->c2, c1->c3]:p0 -- emit[c2, c3]:re
     * values:sv0[->c0] --/
     */
    ASSERT_EQ(graph.size(), 6);
    ASSERT_TRUE(graph.contains(sv0));
    ASSERT_TRUE(graph.contains(sv1));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(re));
    auto&& j0 = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(sv1.output(), j0.left());
    EXPECT_GT(sv0.output(), j0.right());
    auto&& p0 = next<relation::project>(re.input());
    EXPECT_GT(r0.output(), p0.input());

    ASSERT_EQ(sv0.columns().size(), 1);
    auto&& c0m = sv0.columns()[0];
    EXPECT_NE(c0m, c0);

    ASSERT_EQ(sv1.columns().size(), 1);
    auto&& c1m = sv1.columns()[0];
    EXPECT_NE(c1m, c1);

    EXPECT_EQ(r0.condition(), compare(wrap(c0m), wrap(c1m)));

    ASSERT_EQ(p0.columns().size(), 2);
    EXPECT_EQ(p0.columns()[0].value(), wrap(c0m));
    EXPECT_EQ(p0.columns()[0].variable(), c2);
    EXPECT_EQ(p0.columns()[1].value(), wrap(c1m));
    EXPECT_EQ(p0.columns()[1].variable(), c3);

    ASSERT_EQ(re.columns().size(), 2);
    EXPECT_EQ(re.columns()[0].source(), c2);
    EXPECT_EQ(re.columns()[1].source(), c3);
}

TEST_F(expand_subquery_scalar_test, scalar_unary) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::unary>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::unary {
            scalar::unary_operator::plus,
            std::move(subquery),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::unary {
            scalar::unary_operator::plus,
            wrap(output),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_cast) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::cast>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::cast {
            ::takatori::type::int4 {},
            scalar::cast_loss_policy::error,
            std::move(subquery),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::cast {
            ::takatori::type::int4 {},
            scalar::cast_loss_policy::error,
            wrap(output),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_binary_left) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::binary>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::binary {
            scalar::binary_operator::add,
            std::move(subquery),
            constant(2),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::binary {
            scalar::binary_operator::add,
            wrap(output),
            constant(2),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_binary_right) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::binary>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::binary {
            scalar::binary_operator::add,
            constant(1),
            std::move(subquery),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::binary {
            scalar::binary_operator::add,
            constant(1),
            wrap(output),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_compare_left) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::compare>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::compare {
            scalar::comparison_operator::equal,
            std::move(subquery),
            constant(2),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::compare {
            scalar::comparison_operator::equal,
            wrap(output),
            constant(2),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_compare_right) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::compare>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::compare {
            scalar::comparison_operator::equal,
            constant(1),
            std::move(subquery),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::compare {
            scalar::comparison_operator::equal,
            constant(1),
            wrap(output),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_match_input) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::match>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::match {
            scalar::match_operator::like,
            std::move(subquery),
            constant(2),
            constant(3),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::match {
            scalar::match_operator::like,
            wrap(output),
            constant(2),
            constant(3),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_match_pattern) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::match>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::match {
            scalar::match_operator::like,
            constant(1),
            std::move(subquery),
            constant(3),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::match {
            scalar::match_operator::like,
            constant(1),
            wrap(output),
            constant(3),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_match_escape) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::match>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::match {
            scalar::match_operator::like,
            constant(1),
            constant(2),
            std::move(subquery),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::match {
            scalar::match_operator::like,
            constant(1),
            constant(2),
            wrap(output),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_conditional_alternative_condition) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::conditional>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::conditional {
                {
                        scalar::conditional::alternative {
                                std::move(subquery),
                                constant(2),
                        },
                        scalar::conditional::alternative {
                                constant(3),
                                constant(4),
                        },
                },
                {},
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::conditional {
            {
                    scalar::conditional::alternative {
                            wrap(output),
                            constant(2),
                    },
                    scalar::conditional::alternative {
                            constant(3),
                            constant(4),
                    },
            },
            {},
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_conditional_alternative_body) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::conditional>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::conditional {
                {
                        scalar::conditional::alternative {
                                constant(1),
                                constant(2),
                        },
                        scalar::conditional::alternative {
                                constant(3),
                                std::move(subquery),
                        },
                },
                constant(5),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::conditional {
            {
                    scalar::conditional::alternative {
                            constant(1),
                            constant(2),
                    },
                    scalar::conditional::alternative {
                            constant(3),
                            wrap(output),
                    },
            },
            constant(5),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_conditional_default_expression) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::conditional>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::conditional {
                {
                        scalar::conditional::alternative {
                                constant(1),
                                constant(2),
                        },
                        scalar::conditional::alternative {
                                constant(3),
                                constant(4),
                        },
                },
                std::move(subquery),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::conditional {
            {
                    scalar::conditional::alternative {
                            constant(1),
                            constant(2),
                    },
                    scalar::conditional::alternative {
                            constant(3),
                            constant(4),
                    },
            },
            wrap(output),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_coalesce) {
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::coalesce>(output, [](extension::scalar::subquery&& subquery) {
        return scalar::coalesce {
                {
                        constant(1),
                        std::move(subquery),
                        constant(3),
                },
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::coalesce {
            {
                    constant(1),
                    wrap(output),
                    constant(3),
            },
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_let_declarator) {
    auto v0 = bindings.stream_variable("v0");
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::let>(output, [&](extension::scalar::subquery&& subquery) {
        return scalar::let {
                {
                        scalar::let::declarator {
                                v0,
                                std::move(subquery),
                        },
                },
                constant(2),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::let {
            {
                    scalar::let::declarator {
                            v0,
                            wrap(output),
                    },
            },
            constant(2),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_let_body) {
    auto v0 = bindings.stream_variable("v0");
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::let>(output, [&](extension::scalar::subquery&& subquery) {
        return scalar::let {
                {
                        scalar::let::declarator {
                                v0,
                                constant(1),
                        },
                },
                std::move(subquery),
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::let {
            {
                    scalar::let::declarator {
                            v0,
                            constant(1),
                    },
            },
            wrap(output),
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_function_call) {
    auto f = bindings.function(function::declaration {
            1,
            "f",
            ::takatori::type::int4 {},
            {
                    ::takatori::type::int4 {},
                    ::takatori::type::int4 {},
                    ::takatori::type::int4 {},
            },
    });
    auto output = bindings.stream_variable("output");
    auto r = build_scalar<scalar::function_call>(output, [&](extension::scalar::subquery&& subquery) {
        return scalar::function_call {
                f,
                {
                        constant(1),
                        std::move(subquery),
                        constant(3),
                },
        };
    });
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, (scalar::function_call {
            f,
            {
                    constant(1),
                    wrap(output),
                    constant(3),
            },
    }));
}

TEST_F(expand_subquery_scalar_test, scalar_subquery_nesting) {
    /*
     * SELECT
     *   c3,
     *   (
     *     SELECT
     *       (
     *          VALUES (...) AS sv0(c0)
     *       ) AS c2
     *     FROM (VALUES (...)) AS sv1(c1)
     *   ) AS c4
     * FROM (VALUES (...)) AS rv(c3)
     */

    /* inner query: g0
     *   values:sv0 -*
     */
    relation::graph_type g0 {};
    auto c0 = bindings.stream_variable("c0");
    auto&& sv0 = g0.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    /* inner query: g1
     *   values:sv1 -- project[g0->c2]:r0 -- emit:re
     */
    relation::graph_type g1 {};
    auto c1 = bindings.stream_variable("c1");
    auto&& sv1 = g0.insert(relation::values {
            {
                    c1,
            },
            {},
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = g1.insert(relation::project {
            relation::project::column {
                    extension::scalar::subquery { std::move(g0), c0 },
                    c2,
            },
    });
    sv1.output() >> r0.input();

    /* outer query: graph
     *   values:rv -- project[g1->c3]:r1 -- emit:re
     */
    relation::graph_type graph {};
    auto c3 = bindings.stream_variable("c3");
    auto&& rv = graph.insert(relation::values {
            {
                    c3,
            },
            {},
    });
    auto c4 = bindings.stream_variable("c2");
    auto&& r1 = graph.insert(relation::project {
            relation::project::column {
                    extension::scalar::subquery { std::move(g1), c2 },
                    c4,
            },
    });
    auto&& re = graph.insert(relation::emit {
            c3,
            c4,
    });
    rv.output() >> r1.input();
    r1.output() >> re.input();

    auto diagnostics = expand_subquery(graph);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    /*
     * 1values:rv[->c3] ------------------------------------\
     *                                                       join:j1 -- project[c2->c4]:r1 -- emit[c3, c4]:re
     * values:sv1[->c0] --\                                 /
     *                     join:j0 -- project[c0->c2]:r0 --/
     * values:sv0[->c1] --/
     */
    ASSERT_EQ(graph.size(), 8);
    ASSERT_TRUE(graph.contains(sv0));
    ASSERT_TRUE(graph.contains(sv1));
    ASSERT_TRUE(graph.contains(r0));
    ASSERT_TRUE(graph.contains(rv));
    ASSERT_TRUE(graph.contains(r1));
    ASSERT_TRUE(graph.contains(re));
    auto&& j0 = next<relation::intermediate::join>(r0.input());
    EXPECT_GT(sv1.output(), j0.left());
    EXPECT_GT(sv0.output(), j0.right());
    auto&& j1 = next<relation::intermediate::join>(r1.input());
    EXPECT_GT(rv.output(), j1.left());
    EXPECT_GT(r0.output(), j1.right());

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].value(), wrap(c0));
    EXPECT_EQ(r0.columns()[0].variable(), c2);

    ASSERT_EQ(r1.columns().size(), 1);
    EXPECT_EQ(r1.columns()[0].value(), wrap(c2));
    EXPECT_EQ(r1.columns()[0].variable(), c4);
}

} // namespace yugawara::analyzer::details
