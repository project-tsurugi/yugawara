#include "yugawara/analyzer/intermediate_plan_normalizer.h"

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/intermediate/join.h>

#include <takatori/util/vector_print_support.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/extension/scalar/subquery.h>
#include <yugawara/extension/relation/subquery.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::scalar::comparison_operator;
using ::takatori::relation::endpoint_kind;
using ::takatori::util::print_support;

class intermediate_plan_normalizer_test: public ::testing::Test {
protected:
    binding::factory bindings {};

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    std::shared_ptr<storage::table> t0 = storages->add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::table> t1 = storages->add_table({
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];
    storage::column const& t0c2 = t0->columns()[2];
    storage::column const& t1c0 = t1->columns()[0];
    storage::column const& t1c1 = t1->columns()[1];
    storage::column const& t1c2 = t1->columns()[2];

    std::shared_ptr<storage::index> i0 = storages->add_index({ t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages->add_index({ t1, "I1", });
};

TEST_F(intermediate_plan_normalizer_test, relation_subquery) {
    relation::graph_type inner;
    auto c0 = bindings.stream_variable("c0");
    auto&& in = inner.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
            },
    });

    relation::graph_type r;
    auto o0 = bindings.stream_variable("o0");
    auto&& subquery = r.insert(extension::relation::subquery {
            std::move(inner),
            {
                    { c0, o0 },
            },
            true,
    });
    auto&& out = r.insert(relation::emit { o0 });
    subquery.output() >> out.input();

    intermediate_plan_normalizer normalizer {};
    auto diagnostics = normalizer(r);
    ASSERT_TRUE(diagnostics.empty()) << print_support(diagnostics);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(in));
    ASSERT_TRUE(r.contains(out));
    auto&& project = next<relation::project>(in.output());

    ASSERT_EQ(in.columns().size(), 1);
    EXPECT_EQ(in.columns()[0].source(), bindings(t0c0));
    auto&& c0m = in.columns()[0].destination();
    EXPECT_NE(c0m, c0);

    ASSERT_EQ(project.columns().size(), 1);
    EXPECT_EQ(project.columns()[0].value(), scalar::variable_reference { c0m });
    auto&& q0 = project.columns()[0].variable();

    ASSERT_EQ(out.columns().size(), 1);
    EXPECT_EQ(out.columns()[0].source(), q0);
}

TEST_F(intermediate_plan_normalizer_test, scalar_subquery) {
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

    intermediate_plan_normalizer normalizer {};
    auto diagnostics = normalizer(graph);
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
    EXPECT_EQ(r0.columns()[0].value(), scalar::variable_reference { c0 });
    EXPECT_EQ(r0.columns()[0].variable(), c2);

    ASSERT_EQ(re.columns().size(), 2);
    EXPECT_EQ(re.columns()[0].source(), c1);
    EXPECT_EQ(re.columns()[1].source(), c2);
}

} // namespace yugawara::analyzer
