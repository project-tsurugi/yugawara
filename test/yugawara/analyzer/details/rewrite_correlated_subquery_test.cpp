#include <yugawara/analyzer/details/rewrite_correlated_subquery.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>
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
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>

#include <takatori/util/vector_print_support.h>

#include <yugawara/binding/factory.h>

#include <yugawara/function/declaration.h>

#include <yugawara/storage/configurable_provider.h>

#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/scalar/exists.h>
#include <yugawara/extension/scalar/quantified_compare.h>
#include <yugawara/extension/relation/subquery.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::util::print_support;

class rewrite_correlated_subquery_test : public ::testing::Test {
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
};

TEST_F(rewrite_correlated_subquery_test, scalar_subquery) {
    /*
     * subquery: (v0a) -> (v0p)
     *   scan[c0, c1]:r0 -- filter[c0=v0p]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = g.insert(relation::scan {
            bindings(i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& r1 = g.insert(relation::filter {
            compare(c0, v0p),
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * scan[c0, c1]:r0 -\
     *                   +- join:j0 -- filter[c0=v0p]:r0 => c1
     *             () --/
     * --
     */
    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pm = outputs[0].destination();

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    EXPECT_EQ(parameters[0].destination(), v0pm);

    auto&& j0 = downcast<relation::intermediate::join>(inputs[0].input_port().owner());

    EXPECT_GT(r0.output(), j0.right());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.condition(), compare(c0, v0pm));

    EXPECT_EQ(query.output_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, exists) {
    /*
     * exists: (v0a) -> (v0p)
     *   scan[c0, c1]:r0 -- filter[c0=v0p]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = g.insert(relation::scan {
            bindings(i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& r1 = g.insert(relation::filter {
            compare(c0, v0p),
    });
    r0.output() >> r1.input();

    extension::scalar::exists query {
            std::move(g),
            {
                    { v0a, v0p },
            },
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * scan[c0, c1]:r0 -\
     *                   +- join:j0 -- filter[c0=v0p]:r0 => c1
     *             () --/
     * --
     */
    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pm = outputs[0].destination();

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    EXPECT_EQ(parameters[0].destination(), v0pm);

    auto&& j0 = downcast<relation::intermediate::join>(inputs[0].input_port().owner());

    EXPECT_GT(r0.output(), j0.right());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.condition(), compare(c0, v0pm));
}

TEST_F(rewrite_correlated_subquery_test, quantified_compare) {
    /*
     * quantified_compare: (v0a) -> (v0p)
     *   scan[c0, c1]:r0 -- filter[c0=v0p]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = g.insert(relation::scan {
            bindings(i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& r1 = g.insert(relation::filter {
            compare(c0, v0p),
    });
    r0.output() >> r1.input();

    extension::scalar::quantified_compare query {
            scalar::comparison_operator::equal,
            scalar::quantifier::all,
            constant(0),
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * scan[c0, c1]:r0 -\
     *                   +- join:j0 -- filter[c0=v0p]:r0 => c1
     *             () --/
     * --
     */
    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pm = outputs[0].destination();

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    EXPECT_EQ(parameters[0].destination(), v0pm);

    auto&& j0 = downcast<relation::intermediate::join>(inputs[0].input_port().owner());

    EXPECT_GT(r0.output(), j0.right());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.condition(), compare(c0, v0pm));

    EXPECT_EQ(query.right_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, find) {
    /*
     * subquery: (v0a) -> (v0p)
     *   find[c0, c1]:r0 -- filter[c0=v0p]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = g.insert(relation::find {
            bindings(i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
            {
                    relation::find::key { c0, constant(1) },
            },
    });
    auto&& r1 = g.insert(relation::filter {
            compare(c0, v0p),
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * find[c0, c1]:r0 -\
     *                   +- join:j0 -- filter[c0=v0p]:r0 => c1
     *             () --/
     * --
     */
    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::intermediate::join>(inputs[0].input_port().owner());

    EXPECT_GT(r0.output(), j0.right());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.condition(), compare(c0, v0pm));

    EXPECT_EQ(query.output_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, scan) {
    /*
     * subquery: (v0a) -> (v0p)
     *   scan[c0, c1]:r0 -- filter[c0=v0p]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = g.insert(relation::scan {
            bindings(i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
            {
                    relation::scan::key { c0, constant(0) },
                    relation::endpoint_kind::prefixed_inclusive,
            },
            {
                    relation::scan::key { c0, constant(1) },
                    relation::endpoint_kind::prefixed_inclusive,
            },
    });
    auto&& r1 = g.insert(relation::filter {
            compare(c0, v0p),
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * scan[c0, c1]:r0 -\
     *                   +- join:j0 -- filter[c0=v0p]:r0 => c1
     *             () --/
     * --
     */
    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::intermediate::join>(inputs[0].input_port().owner());

    EXPECT_GT(r0.output(), j0.right());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.condition(), compare(c0, v0pm));

    EXPECT_EQ(query.output_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, values) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 => c0
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 => c0
     */
    ASSERT_EQ(query.query_graph().size(), 1);

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());

    EXPECT_FALSE(j0.input().opposite());
    EXPECT_FALSE(j0.output().opposite());

    auto&& columns = j0.columns();
    ASSERT_EQ(columns.size(), 1);
    EXPECT_EQ(columns[0].variable(), c0);
    EXPECT_EQ(columns[0].value(), varref(v0pm));

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, values_empty_rows) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 => c0
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    g.insert(relation::values {
            {
                    c0,
            },
            {},
    });

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_FALSE(result);
    EXPECT_TRUE(contains(result.diagnostics(), code_type::unsupported_feature)) << print_support(result.diagnostics());
}

TEST_F(rewrite_correlated_subquery_test, values_multiple_rows) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 => c0
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
                    { constant(0) },
            },
    });

    extension::scalar::subquery query {
        std::move(g),
        {
                        { v0a, v0p },
                },
                c0,
        };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_FALSE(result);
    EXPECT_TRUE(contains(result.diagnostics(), code_type::unsupported_feature)) << print_support(result.diagnostics());
}

// join_find, join_scan are not appear in here

TEST_F(rewrite_correlated_subquery_test, apply) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- apply[f(v0p)->(c1)]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant() },
            },
    });

    auto c1 = bindings.stream_variable("c1");
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
    auto&& r1 = g.insert(relation::apply {
            tvf,
            {
                    varref(v0p),
            },
            {
                    { 0, c1, }
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -- apply:r1 => c1
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.arguments()[0], varref(v0pm));

    EXPECT_EQ(query.output_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, project) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- project[c1:=v0p]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant() },
            },
    });

    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::project {
            {
                    relation::project::column { c1, varref(v0p) }
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -- project:r1 => c1
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.columns()[0].value(), varref(v0pm));

    EXPECT_EQ(query.output_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, filter) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- filter[c0=v0p]:r1 => c0
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant() },
            },
    });
    auto&& r1 = g.insert(relation::filter {
            compare(c0, v0p),
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -- filter:r1 => c1
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    EXPECT_EQ(r1.condition(), compare(c0, v0pm));

    EXPECT_EQ(query.output_column(), c0);
}

// FIXME: check buffer operator

// NOTE: skip test for emit and write operator, because current subqueries cannot appear

TEST_F(rewrite_correlated_subquery_test, join_relation_inner_left_parameter) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- join[c0 = c1] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { constant(1) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(c0, c1),
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -\
     *                             +- join:r2 => c0
     *             values[c1]:r1 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r1));
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());
    EXPECT_EQ(r2.condition(), compare(c0, c1));

    auto&& columns = j0.columns();
    ASSERT_EQ(columns.size(), 1);
    EXPECT_EQ(columns[0].variable(), c0);
    EXPECT_EQ(columns[0].value(), varref(v0pm));

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, join_relation_inner_right_parameter) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- join[c0 = c1] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant(1) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { varref(v0p) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(c0, c1),
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     *             values[c0]:r0 -\
     *                             +- join:r2 => c0
     * () -- project[c1:=v0p]:j0 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r0));
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.right());
    EXPECT_EQ(r2.condition(), compare(c0, c1));

    auto&& columns = j0.columns();
    ASSERT_EQ(columns.size(), 1);
    EXPECT_EQ(columns[0].variable(), c1);
    EXPECT_EQ(columns[0].value(), varref(v0pm));

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, join_relation_inner_condition_parameter) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- join[c0 + v0p = c1] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant(0) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { constant(1) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(
                    scalar::binary(scalar::binary_operator::add, varref(c0), varref(v0p)),
                    varref(c1)),
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=0]:j0 -\
     *                           +- join:r2 => c0
     *           values[c1]:r1 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r1));
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());

    EXPECT_EQ(r2.condition(), compare(
            scalar::binary(scalar::binary_operator::add, varref(c0), varref(v0pm)),
            varref(c1)));

    auto&& columns = j0.columns();
    ASSERT_EQ(columns.size(), 1);
    EXPECT_EQ(columns[0].variable(), c0);
    EXPECT_EQ(columns[0].value(), constant(0));

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, join_relation_inner_both_parameter) {
    /*
     * subquery: (v0a, v1a) -> (v0p, v1p)
     *   values[c0]:r0 -\
     *                   +- join[c0 = c1] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v1a = bindings.stream_variable("v1a");
    auto v0p = bindings.frame_variable("v0p");
    auto v1p = bindings.frame_variable("v1p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { varref(v1p) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(c0, c1),
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
                    { v1a, v1p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -\
     *                             +- join:r2 => c0
     * () -- project[c1:=v1p]:j1 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 2);

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 2);
    EXPECT_EQ(outputs[0].source(), v0p);
    EXPECT_EQ(outputs[1].source(), v1p);
    auto&& v0pm0 = outputs[0].destination();
    auto&& v1pm0 = outputs[1].destination();

    ASSERT_EQ(inputs[0].mappings().size(), 2);
    EXPECT_EQ(inputs[0].mappings()[0].source(), v0p);
    EXPECT_EQ(inputs[0].mappings()[0].destination(), v0pm0);
    EXPECT_EQ(inputs[0].mappings()[1].source(), v1p);
    EXPECT_EQ(inputs[0].mappings()[1].destination(), v1pm0);

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());

    ASSERT_EQ(j0.columns().size(), 1);
    EXPECT_EQ(j0.columns()[0].variable(), c0);
    EXPECT_EQ(j0.columns()[0].value(), varref(v0pm0));

    ASSERT_EQ(inputs[1].mappings().size(), 2);
    EXPECT_EQ(inputs[1].mappings()[0].source(), v0p);
    auto&& v0pm1 = inputs[1].mappings()[0].destination();
    EXPECT_EQ(inputs[1].mappings()[1].source(), v1p);
    auto&& v1pm1 = inputs[1].mappings()[1].destination();

    auto&& j1 = downcast<relation::project>(inputs[1].input_port().owner());
    EXPECT_FALSE(j1.input().opposite());
    EXPECT_GT(j1.output(), r2.right());

    ASSERT_EQ(j1.columns().size(), 1);
    EXPECT_EQ(j1.columns()[0].variable(), c1);
    EXPECT_EQ(j1.columns()[0].value(), varref(v1pm1));

    EXPECT_EQ(r2.condition(), land(
            land(
                    compare(c0, c1),
                    compare(v0pm0, v0pm1, scalar::comparison_operator::is_not_distinct_from)),
            compare(v1pm0, v1pm1, scalar::comparison_operator::is_not_distinct_from)));

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, join_relation_left_outer_left_parameter) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- join[c0 = c1] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { constant(1) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(c0, c1),
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -\
     *                             +- join:r2 => c0
     *             values[c1]:r1 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r1));
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());
    EXPECT_EQ(r2.condition(), compare(c0, c1));

    auto&& columns = j0.columns();
    ASSERT_EQ(columns.size(), 1);
    EXPECT_EQ(columns[0].variable(), c0);
    EXPECT_EQ(columns[0].value(), varref(v0pm));

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, join_relation_left_outer_right_parameter) {
    /*
     * subquery: (v0a, v1a) -> (v0p, v1p)
     *   values[c0]:r0 -\
     *                   +- join[c0 = c1] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant(0) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { varref(v0p) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(c0, c1),
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     *   () -- project[c0:=0]:j0 -\
     *                             +- join:r2 => c0
     * () -- project[c1:=v1p]:j1 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 2);

    ASSERT_EQ(inputs[0].mappings().size(), 1);
    EXPECT_EQ(inputs[0].mappings()[0].source(), v0p);
    auto&& v0pm0 = inputs[0].mappings()[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());

    ASSERT_EQ(j0.columns().size(), 1);
    EXPECT_EQ(j0.columns()[0].variable(), c0);
    EXPECT_EQ(j0.columns()[0].value(), constant(0));

    ASSERT_EQ(inputs[1].mappings().size(), 1);
    EXPECT_EQ(inputs[1].mappings()[0].source(), v0p);
    auto&& v0pm1 = inputs[1].mappings()[0].destination();

    auto&& j1 = downcast<relation::project>(inputs[1].input_port().owner());
    EXPECT_FALSE(j1.input().opposite());
    EXPECT_GT(j1.output(), r2.right());

    ASSERT_EQ(j1.columns().size(), 1);
    EXPECT_EQ(j1.columns()[0].variable(), c1);
    EXPECT_EQ(j1.columns()[0].value(), varref(v0pm1));

    EXPECT_EQ(r2.condition(), land(
            compare(c0, c1),
            compare(v0pm0, v0pm1, scalar::comparison_operator::is_not_distinct_from)));

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, join_relation_full_outer) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- join[c0 = c1] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { constant(1) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            compare(c0, c1),
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_FALSE(result) << print_support(result.diagnostics());
    EXPECT_TRUE(contains(result.diagnostics(), code_type::unsupported_feature)) << print_support(result.diagnostics());
}

TEST_F(rewrite_correlated_subquery_test, aggregate_grouped) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0, c1]:r0 -- aggregate[by:c0, c2:=f(c1, v0p)]:r1 => c2
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = g.insert(relation::values {
            {
                    c0, c1,
            },
            {
                    { constant(0), constant(1) },
            },
    });

    auto c2 = bindings.stream_variable("c2");
    auto&& sf = bindings.aggregate_function({
            aggregate::declaration::minimum_user_function_id + 1,
            "sf",
            ::takatori::type::int4 {},
            {
                    ::takatori::type::int4 {},
                    ::takatori::type::int4 {},
            },
    });
    auto&& r1 = g.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    {
                            sf,
                            { c1, v0p },
                            c2,
                    },
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c2,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -- aggregate:r1 => c2
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    ASSERT_EQ(r1.group_keys().size(), 2);
    EXPECT_EQ(r1.group_keys()[0], c0);
    EXPECT_EQ(r1.group_keys()[1], v0pm);

    EXPECT_EQ(r1.columns()[0].arguments()[0], c1);
    EXPECT_EQ(r1.columns()[0].arguments()[1], v0pm);

    EXPECT_EQ(query.output_column(), c2);
}

TEST_F(rewrite_correlated_subquery_test, aggregate_scalar) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- aggregate[c1:=f(c0, v0p)]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant() },
            },
    });

    auto c1 = bindings.stream_variable("c1");
    auto&& sf = bindings.aggregate_function({
            aggregate::declaration::minimum_user_function_id + 1,
            "sf",
            ::takatori::type::int4 {},
            {
                    ::takatori::type::int4 {},
                    ::takatori::type::int4 {},
            },
    });
    auto&& r1 = g.insert(relation::intermediate::aggregate {
            {},
            {
                    {
                            sf,
                            { c0, v0p },
                            c1,
                    },
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(contains(result.diagnostics(), code_type::unsupported_feature)) << print_support(result.diagnostics());
}

TEST_F(rewrite_correlated_subquery_test, distinct) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- distinct[by:c0]:r1 => c0
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });

    auto c2 = bindings.stream_variable("c2");
    auto&& r1 = g.insert(relation::intermediate::distinct {
            {
                    c0,
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -- distinct:r1 => c0
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    ASSERT_EQ(r1.group_keys().size(), 2);
    EXPECT_EQ(r1.group_keys()[0], c0);
    EXPECT_EQ(r1.group_keys()[1], v0pm);

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, limit) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- limit[by:c0, sort:v0p]:r1 => c0
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant() },
            },
    });

    auto c2 = bindings.stream_variable("c2");
    auto&& r1 = g.insert(relation::intermediate::limit {
            {},
            {
                    c0,
            },
            {
                    { v0p }
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -- distinct:r1 => c0
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    ASSERT_EQ(r1.group_keys().size(), 2);
    EXPECT_EQ(r1.group_keys()[0], c0);
    EXPECT_EQ(r1.group_keys()[1], v0pm);

    ASSERT_EQ(r1.sort_keys().size(), 1);
    EXPECT_EQ(r1.sort_keys()[0].variable(), v0pm);

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, union_left) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- union[c2:=(c0, c1)] => c2
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { constant(1) },
            },
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& r2 = g.insert(relation::intermediate::union_ {
            {
                    { c0, c1, c2 },
            },
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c2,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -\
     *                             +- union:r2 => c2
     *   () -- project[c0:=1]:j0 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 2);

    ASSERT_EQ(inputs[0].mappings().size(), 1);
    EXPECT_EQ(inputs[0].mappings()[0].source(), v0p);
    auto&& v0pm0 = inputs[0].mappings()[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());

    ASSERT_EQ(j0.columns().size(), 1);
    EXPECT_EQ(j0.columns()[0].variable(), c0);
    EXPECT_EQ(j0.columns()[0].value(), varref(v0pm0));

    ASSERT_EQ(inputs[1].mappings().size(), 1);
    EXPECT_EQ(inputs[1].mappings()[0].source(), v0p);
    auto&& v0pm1 = inputs[1].mappings()[0].destination();

    auto&& j1 = downcast<relation::project>(inputs[1].input_port().owner());
    EXPECT_FALSE(j1.input().opposite());
    EXPECT_GT(j1.output(), r2.right());

    ASSERT_EQ(j1.columns().size(), 1);
    EXPECT_EQ(j1.columns()[0].variable(), c1);
    EXPECT_EQ(j1.columns()[0].value(), constant(1));

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pmo = outputs[0].destination();
    EXPECT_NE(v0pmo, v0pm0);
    EXPECT_NE(v0pmo, v0pm1);

    auto&& mappings = r2.mappings();
    ASSERT_EQ(mappings.size(), 2);
    EXPECT_EQ(mappings[0].left(), c0);
    EXPECT_EQ(mappings[0].right(), c1);
    EXPECT_EQ(mappings[0].destination(), c2);
    EXPECT_EQ(mappings[1].left(), v0pm0);
    EXPECT_EQ(mappings[1].right(), v0pm1);
    EXPECT_EQ(mappings[1].destination(), v0pmo);

    auto&& columns = j0.columns();
    ASSERT_EQ(columns.size(), 1);
    EXPECT_EQ(columns[0].variable(), c0);
    EXPECT_EQ(columns[0].value(), varref(v0pm0));

    EXPECT_EQ(query.output_column(), c2);
}

TEST_F(rewrite_correlated_subquery_test, union_right) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- union[c2:=(c0, c1)] => c2
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant(0) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { varref(v0p) },
            },
    });
    auto c2 = bindings.stream_variable("c2");
    auto&& r2 = g.insert(relation::intermediate::union_ {
            {
                    { c0, c1, c2 },
            },
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c2,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     *   () -- project[c0:=0]:j0 -\
     *                             +- union:r2 => c2
     * () -- project[c0:=v0p]:j0 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 2);

    ASSERT_EQ(inputs[0].mappings().size(), 1);
    EXPECT_EQ(inputs[0].mappings()[0].source(), v0p);
    auto&& v0pm0 = inputs[0].mappings()[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());

    ASSERT_EQ(j0.columns().size(), 1);
    EXPECT_EQ(j0.columns()[0].variable(), c0);
    EXPECT_EQ(j0.columns()[0].value(), constant(0));

    ASSERT_EQ(inputs[1].mappings().size(), 1);
    EXPECT_EQ(inputs[1].mappings()[0].source(), v0p);
    auto&& v0pm1 = inputs[1].mappings()[0].destination();

    auto&& j1 = downcast<relation::project>(inputs[1].input_port().owner());
    EXPECT_FALSE(j1.input().opposite());
    EXPECT_GT(j1.output(), r2.right());

    ASSERT_EQ(j1.columns().size(), 1);
    EXPECT_EQ(j1.columns()[0].variable(), c1);
    EXPECT_EQ(j1.columns()[0].value(), varref(v0pm1));

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pmo = outputs[0].destination();
    EXPECT_NE(v0pmo, v0pm0);
    EXPECT_NE(v0pmo, v0pm1);

    auto&& mappings = r2.mappings();
    ASSERT_EQ(mappings.size(), 2);
    EXPECT_EQ(mappings[0].left(), c0);
    EXPECT_EQ(mappings[0].right(), c1);
    EXPECT_EQ(mappings[0].destination(), c2);
    EXPECT_EQ(mappings[1].left(), v0pm0);
    EXPECT_EQ(mappings[1].right(), v0pm1);
    EXPECT_EQ(mappings[1].destination(), v0pmo);

    EXPECT_EQ(query.output_column(), c2);
}

TEST_F(rewrite_correlated_subquery_test, intersection_left) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- intersect[(c0, c1)] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { constant(1) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::intersection {
            {
                    { c0, c1 },
            },
            relation::set_quantifier::distinct,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- project[c0:=v0p]:j0 -\
     *                             +- intersection:r2 => c0
     *             values[c1]:r1 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r1));
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    ASSERT_EQ(inputs[0].mappings().size(), 1);
    EXPECT_EQ(inputs[0].mappings()[0].source(), v0p);
    auto&& v0pm0 = inputs[0].mappings()[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());

    ASSERT_EQ(j0.columns().size(), 1);
    EXPECT_EQ(j0.columns()[0].variable(), c0);
    EXPECT_EQ(j0.columns()[0].value(), varref(v0pm0));

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pmo = outputs[0].destination();
    EXPECT_EQ(v0pmo, v0pm0);

    auto&& pairs = r2.group_key_pairs();
    ASSERT_EQ(pairs.size(), 1);
    EXPECT_EQ(pairs[0].left(), c0);
    EXPECT_EQ(pairs[0].right(), c1);

    EXPECT_EQ(query.output_column(), c0);
}

TEST_F(rewrite_correlated_subquery_test, intersection_right) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -\
     *                   +- intersect[(c0, c1)] => c0
     *   values[c1]:r1 -/
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant(0) },
            },
    });
    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::values {
            {
                    c1,
            },
            {
                    { varref(v0p) },
            },
    });
    auto&& r2 = g.insert(relation::intermediate::intersection {
            {
                    { c0, c1 },
            },
            relation::set_quantifier::distinct,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     *   () -- project[c0:=0]:j0 -\
     *                             +- intersection:r2 => c0
     * () -- project[c0:=v0p]:j0 -/
     */
    ASSERT_EQ(query.query_graph().size(), 3);
    ASSERT_TRUE(query.query_graph().contains(r2));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 2);

    ASSERT_EQ(inputs[0].mappings().size(), 1);
    EXPECT_EQ(inputs[0].mappings()[0].source(), v0p);
    auto&& v0pm0 = inputs[0].mappings()[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r2.left());

    ASSERT_EQ(j0.columns().size(), 1);
    EXPECT_EQ(j0.columns()[0].variable(), c0);
    EXPECT_EQ(j0.columns()[0].value(), constant(0));

    ASSERT_EQ(inputs[1].mappings().size(), 1);
    EXPECT_EQ(inputs[1].mappings()[0].source(), v0p);
    auto&& v0pm1 = inputs[1].mappings()[0].destination();

    auto&& j1 = downcast<relation::project>(inputs[1].input_port().owner());
    EXPECT_FALSE(j1.input().opposite());
    EXPECT_GT(j1.output(), r2.right());

    ASSERT_EQ(j1.columns().size(), 1);
    EXPECT_EQ(j1.columns()[0].variable(), c1);
    EXPECT_EQ(j1.columns()[0].value(), varref(v0pm1));

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pmo = outputs[0].destination();
    EXPECT_EQ(v0pmo, v0pm0);

    auto&& pairs = r2.group_key_pairs();
    ASSERT_EQ(pairs.size(), 2);
    EXPECT_EQ(pairs[0].left(), c0);
    EXPECT_EQ(pairs[0].right(), c1);
    EXPECT_EQ(pairs[1].left(), v0pm0);
    EXPECT_EQ(pairs[1].right(), v0pm1);

    EXPECT_EQ(query.output_column(), c0);
}

// NOTE: skip difference because it is same as the intersection

TEST_F(rewrite_correlated_subquery_test, escape) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- escape[c1:=c0]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { varref(v0p) },
            },
    });

    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::intermediate::escape {
            {
                    { c0, c1 },
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- escape[c0:=c1]:j0 -- project:r1 => c1
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm0 = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pmo = outputs[0].destination();
    EXPECT_NE(v0pmo, v0pm0);

    auto&& mappings = r1.mappings();
    ASSERT_EQ(mappings.size(), 2);
    EXPECT_EQ(mappings[0].source(), c0);
    EXPECT_EQ(mappings[0].destination(), c1);
    EXPECT_EQ(mappings[1].source(), v0pm0);
    EXPECT_EQ(mappings[1].destination(), v0pmo);

    EXPECT_EQ(query.output_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, escape_existing) {
    /*
     * subquery: (v0a) -> (v0p)
     *   values[c0]:r0 -- escape[c1:=v0p]:r1 => c1
     */
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(relation::values {
            {
                    c0,
            },
            {
                    { constant(0) },
            },
    });

    auto c1 = bindings.stream_variable("c1");
    auto&& r1 = g.insert(relation::intermediate::escape {
            {
                    { v0p, c1 },
            },
    });
    r0.output() >> r1.input();

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c1,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- escape[c0:=c1]:j0 -- project:r1 => c1
     */
    ASSERT_EQ(query.query_graph().size(), 2);
    ASSERT_TRUE(query.query_graph().contains(r1));

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm0 = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_FALSE(j0.input().opposite());
    EXPECT_GT(j0.output(), r1.input());

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    EXPECT_EQ(outputs[0].destination(), c1);

    auto&& mappings = r1.mappings();
    ASSERT_EQ(mappings.size(), 1);
    EXPECT_EQ(mappings[0].source(), v0pm0);
    EXPECT_EQ(mappings[0].destination(), c1);

    EXPECT_EQ(query.output_column(), c1);
}

TEST_F(rewrite_correlated_subquery_test, relation_subquery) {
    auto v0a = bindings.stream_variable("v0a");
    auto v0p = bindings.frame_variable("v0p");

    /* relation-subquery:g0
     *   values[c0]:g0r0 --*
     */
    relation::graph_type g0 {};
    auto g0c0 = bindings.stream_variable("g0c0");
    g0.insert(relation::values {
            {
                    g0c0,
            },
            {
                    { varref(v0p) },
            },
    });

    /*
     * subquery: (v0a) -> (v0p)
     *   relation-subquery[c0]:r0 => c0
     */
    relation::graph_type g;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = g.insert(extension::relation::subquery {
            std::move(g0),
            {
                    { g0c0, c0 },
            },
    });

    extension::scalar::subquery query {
            std::move(g),
            {
                    { v0a, v0p },
            },
            c0,
    };

    auto result = rewrite_correlated_subquery(query);
    ASSERT_TRUE(result) << print_support(result.diagnostics());

    /*
     * () -- relation-subquery:r0 => c0
     */
    ASSERT_EQ(query.query_graph().size(), 1);
    ASSERT_TRUE(query.query_graph().contains(r0));

    /* in g0:
     *   () -- project[c0:=v0p]:j0 => c0
     */
    auto&& g0m = r0.query_graph();
    ASSERT_EQ(g0m.size(), 1);

    auto&& inputs = result.inputs();
    ASSERT_EQ(inputs.size(), 1);

    auto&& parameters = inputs[0].mappings();
    ASSERT_EQ(parameters.size(), 1);
    EXPECT_EQ(parameters[0].source(), v0p);
    auto&& v0pm = parameters[0].destination();

    auto&& j0 = downcast<relation::project>(inputs[0].input_port().owner());
    EXPECT_TRUE(g0m.contains(j0));

    EXPECT_FALSE(j0.input().opposite());
    EXPECT_FALSE(j0.output().opposite());

    ASSERT_EQ(j0.columns().size(), 1);
    EXPECT_EQ(j0.columns()[0].variable(), g0c0);
    EXPECT_EQ(j0.columns()[0].value(), varref(v0pm));

    auto&& outputs = result.output_mappings();
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_EQ(outputs[0].source(), v0p);
    auto&& v0pmo = outputs[0].destination();

    ASSERT_EQ(r0.mappings().size(), 2);
    EXPECT_EQ(r0.mappings()[0].source(), g0c0);
    EXPECT_EQ(r0.mappings()[0].destination(), c0);
    EXPECT_EQ(r0.mappings()[1].source(), v0pm);
    EXPECT_EQ(r0.mappings()[1].destination(), v0pmo);

    EXPECT_EQ(query.output_column(), c0);
}



} // namespace yugawara::analyzer::details
