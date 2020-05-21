#include <yugawara/analyzer/intermediate_plan_optimizer.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/join_scan.h>

#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/join.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>
#include <takatori/relation/find.h>
#include <takatori/relation/step/join.h>

#include "details/utils.h"

namespace yugawara::analyzer {

// import utils
using namespace details;

using ::takatori::scalar::unary;
using ::takatori::scalar::binary;
using ::takatori::scalar::unary_operator;
using ::takatori::scalar::binary_operator;
using ::takatori::scalar::comparison_operator;

using ::takatori::relation::endpoint_kind;

class intermediate_plan_optimizer_test: public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    binding::factory bindings { creator };

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    std::shared_ptr<storage::table> t0 = storages->add_table("T0", {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::table> t1 = storages->add_table("T1", {
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

    std::shared_ptr<storage::index> i0 = storages->add_index("I0", { t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages->add_index("I1", { t1, "I1", });

    details::intermediate_plan_optimizer_options options() {
        details::intermediate_plan_optimizer_options result;
        result.storage_provider(storages);
        return result;
    }
};

TEST_F(intermediate_plan_optimizer_test, simple) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });
    in.output() >> out.input();

    intermediate_plan_optimizer optimizer { options() };
    optimizer(r);

    ASSERT_EQ(r.size(), 2);
    ASSERT_TRUE(r.contains(in));
    ASSERT_TRUE(r.contains(out));

    ASSERT_EQ(in.columns().size(), 1);
    EXPECT_EQ(in.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(in.columns()[0].destination(), c0);

    ASSERT_EQ(out.columns().size(), 1);
    EXPECT_EQ(out.columns()[0].source(), c0);
}

TEST_F(intermediate_plan_optimizer_test, remove_redundant_stream_variables) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });
    in.output() >> out.input();

    intermediate_plan_optimizer optimizer { options() };
    optimizer(r);

    ASSERT_EQ(r.size(), 2);
    ASSERT_TRUE(r.contains(in));
    ASSERT_TRUE(r.contains(out));

    ASSERT_EQ(in.columns().size(), 1);
    EXPECT_EQ(in.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(in.columns()[0].destination(), c0);

    ASSERT_EQ(out.columns().size(), 1);
    EXPECT_EQ(out.columns()[0].source(), c0);
}

TEST_F(intermediate_plan_optimizer_test, push_down_selections) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& distinct = r.insert(relation::intermediate::distinct {
            { c0, c1 },
    });
    auto&& filter = r.insert(relation::filter {
            compare(c0, c1),
    });

    in.output() >> distinct.input();
    distinct.output() >> filter.input();
    filter.output() >> out.input();

    intermediate_plan_optimizer optimizer { options() };
    optimizer(r);

    ASSERT_EQ(r.size(), 4);
    ASSERT_TRUE(r.contains(in));
    ASSERT_TRUE(r.contains(out));
    ASSERT_TRUE(r.contains(distinct));
    auto&& f = next<relation::filter>(in.output());
    EXPECT_GT(f.output(), distinct.input());
    EXPECT_EQ(f.condition(), compare(c0, c1));
}

TEST_F(intermediate_plan_optimizer_test, rewrite_join_operator) {
    // SELECT T0.cl1, T1.cr1
    // FROM T0
    // INNER JOIN T1
    // WHERE 0 < T1.cr0
    //   AND T1.cr0 <= T0.cl0
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto&& in_left = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
                    { bindings(t0c2), cl2 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& in_right = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
                    { bindings(t1c2), cr2 },
            },
    });
    auto&& out = r.insert(relation::emit { cl1, cr1 });

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& filter = r.insert(relation::filter {
            land(
                    compare(constant(0), varref(cr0), comparison_operator::less),
                    compare(varref(cr0), varref(cl0), comparison_operator::less_equal)),
    });

    in_left.output() >> join.left();
    in_right.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    storages->add_index("x1", storage::index {
            t1,
            "x1",
            { t1c0 },
    });
    intermediate_plan_optimizer optimizer { options() };
    optimizer(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(in_left));
    ASSERT_TRUE(r.contains(out));

    auto&& j = next<relation::join_scan>(in_left.output());
    EXPECT_GT(j.output(), out.input());

    ASSERT_EQ(j.columns().size(), 2);
    EXPECT_EQ(j.columns()[0].source(), bindings(t1c0));
    EXPECT_EQ(j.columns()[1].source(), bindings(t1c1));
    EXPECT_EQ(j.columns()[0].destination(), cr0);
    EXPECT_EQ(j.columns()[1].destination(), cr1);

    EXPECT_EQ(j.lower().kind(), endpoint_kind::prefixed_exclusive);
    ASSERT_EQ(j.lower().keys().size(), 1);
    ASSERT_EQ(j.lower().keys()[0].variable(), j.columns()[0].source());
    ASSERT_EQ(j.lower().keys()[0].value(), constant(0));

    EXPECT_EQ(j.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(j.upper().keys().size(), 1);
    ASSERT_EQ(j.upper().keys()[0].variable(), j.columns()[0].source());
    ASSERT_EQ(j.upper().keys()[0].value(), varref(cl0));

    EXPECT_EQ(j.condition(), nullptr);
}

TEST_F(intermediate_plan_optimizer_test, rewrite_join_condition) {
    // SELECT T0.cl1, T1.cr1
    // FROM T0
    // INNER JOIN T1
    // WHERE T0.cl0 = T1.cr0
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto&& in_left = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
                    { bindings(t0c2), cl2 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& in_right = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
                    { bindings(t1c2), cr2 },
            },
    });
    auto&& out = r.insert(relation::emit { cl1, cr1 });

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& filter = r.insert(relation::filter {
            compare(cl0, cr0),
    });

    in_left.output() >> join.left();
    in_right.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    intermediate_plan_optimizer optimizer { options() };
    optimizer(r);

    ASSERT_EQ(r.size(), 4);
    ASSERT_TRUE(r.contains(in_left));
    ASSERT_TRUE(r.contains(in_right));
    ASSERT_TRUE(r.contains(join));
    ASSERT_TRUE(r.contains(out));
    EXPECT_GT(join.output(), out.input());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    ASSERT_EQ(join.lower().keys()[0].variable(), cr0);
    ASSERT_EQ(join.lower().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    ASSERT_EQ(join.upper().keys()[0].variable(), cr0);
    ASSERT_EQ(join.upper().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.condition(), nullptr);
}

TEST_F(intermediate_plan_optimizer_test, rewrite_join_suppress_index_join) {
    // SELECT T0.cl1, T1.cr1
    // FROM T0
    // INNER JOIN T1
    // WHERE T0.cl0 = T0.cr0
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto&& in_left = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
                    { bindings(t0c2), cl2 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& in_right = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
                    { bindings(t1c2), cr2 },
            },
    });
    auto&& out = r.insert(relation::emit { cl1, cr1 });

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& filter = r.insert(relation::filter {
            compare(varref(cl0), varref(cr0)),
    });

    in_left.output() >> join.left();
    in_right.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    storages->add_index("x1", storage::index {
            t1,
            "x1",
            { t1c0 },
    });
    intermediate_plan_optimizer optimizer { options() };
    auto&& rf = optimizer.options().runtime_features();
    rf.erase(runtime_feature::index_join);
    optimizer(r);

    ASSERT_EQ(r.size(), 4);
    ASSERT_TRUE(r.contains(in_left));
    ASSERT_TRUE(r.contains(in_right));
    ASSERT_TRUE(r.contains(join));
    ASSERT_TRUE(r.contains(out));

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    ASSERT_EQ(join.lower().keys()[0].variable(), cr0);
    ASSERT_EQ(join.lower().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    ASSERT_EQ(join.upper().keys()[0].variable(), cr0);
    ASSERT_EQ(join.upper().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.condition(), nullptr);
}

TEST_F(intermediate_plan_optimizer_test, rewrite_join_suppress_broadcast) {
    // SELECT T0.cl1, T1.cr1
    // FROM T0
    // INNER JOIN T1
    // WHERE T0.cl0 = T1.cr0
    // AND T0.cl1 < T1.cr1
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto&& in_left = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
                    { bindings(t0c2), cl2 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& in_right = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
                    { bindings(t1c2), cr2 },
            },
    });
    auto&& out = r.insert(relation::emit { cl2, cr2 });

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& filter = r.insert(relation::filter {
            land(
                    compare(cl0, cr0),
                    compare(cl1, cr1, comparison_operator::less))
    });

    in_left.output() >> join.left();
    in_right.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    intermediate_plan_optimizer optimizer { options() };
    auto&& rf = optimizer.options().runtime_features();
    rf.erase(runtime_feature::broadcast_exchange);
    optimizer(r);

    ASSERT_EQ(r.size(), 4);
    ASSERT_TRUE(r.contains(in_left));
    ASSERT_TRUE(r.contains(in_right));
    ASSERT_TRUE(r.contains(join));
    ASSERT_TRUE(r.contains(out));
    EXPECT_GT(join.output(), out.input());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    ASSERT_EQ(join.lower().keys()[0].variable(), cr0);
    ASSERT_EQ(join.lower().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    ASSERT_EQ(join.upper().keys()[0].variable(), cr0);
    ASSERT_EQ(join.upper().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.condition(), compare(cl1, cr1, comparison_operator::less));
}

TEST_F(intermediate_plan_optimizer_test, rewrite_scan) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& distinct = r.insert(relation::intermediate::distinct {
            { c0, c1 },
    });
    auto&& filter = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });

    in.output() >> distinct.input();
    distinct.output() >> filter.input();
    filter.output() >> out.input();

    storages->add_index("x0", {
            t0, "x0",
            { t0c0 },
    });
    intermediate_plan_optimizer optimizer { options() };
    optimizer(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(out));
    ASSERT_TRUE(r.contains(distinct));
    auto&& find = next<relation::find>(distinct.input());

    ASSERT_EQ(find.columns().size(), 2);
    EXPECT_EQ(find.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(find.columns()[1].source(), bindings(t0c1));
    EXPECT_EQ(find.columns()[0].destination(), c0);
    EXPECT_EQ(find.columns()[1].destination(), c1);

    ASSERT_EQ(find.keys().size(), 1);
    EXPECT_EQ(find.keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(find.keys()[0].value(), constant(0));
}

} // namespace yugawara::analyzer
