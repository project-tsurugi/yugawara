#include <yugawara/analyzer/details/rewrite_join.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/distinct.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/analyzer/details/default_index_estimator.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::scalar::comparison_operator;

class rewrite_join_test: public ::testing::Test {
protected:
    binding::factory bindings {};

    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];
    storage::column const& t0c2 = t0->columns()[2];

    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });

    std::shared_ptr<storage::table> t1 = storages.add_table({
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t1c0 = t1->columns()[0];
    storage::column const& t1c1 = t1->columns()[1];
    storage::column const& t1c2 = t1->columns()[2];

    std::shared_ptr<storage::index> i1 = storages.add_index({ t1, "I1", });

    void apply(relation::graph_type& graph) {
        apply_custom(graph, true);
    }

    void apply_custom(relation::graph_type& graph, bool enable_join_scan) {
        default_index_estimator estimator;
        flow_volume_info vinfo {};
        rewrite_join(graph, estimator, vinfo, enable_join_scan);
    }
};

template<class T, class U = T>
static bool eq(::takatori::util::sequence_view<T const> actual, std::initializer_list<U const> expect) {
    return std::equal(actual.begin(), actual.end(), expect.begin(), expect.end());
}

TEST_F(rewrite_join_test, stay) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_EQ(result.condition(), compare(cl0, cr0));
}

TEST_F(rewrite_join_test, right_direct) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 3);

    auto&& result = next<relation::join_find>(out.input());
    EXPECT_GT(inl.output(), result.left());

    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 1);
    EXPECT_EQ(result.columns()[0].source(), bindings(t1c0));
    EXPECT_EQ(result.columns()[0].destination(), cr0);

    ASSERT_EQ(result.keys().size(), 1);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t1c0));
    EXPECT_EQ(result.keys()[0].value(), varref(cl0));

    EXPECT_EQ(result.condition(), nullptr);
}

TEST_F(rewrite_join_test, left_direct) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t0,
            "x0",
            {
                    t0->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 3);

    auto&& result = next<relation::join_find>(out.input());
    EXPECT_GT(inr.output(), result.left());

    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 1);
    EXPECT_EQ(result.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(result.columns()[0].destination(), cl0);

    ASSERT_EQ(result.keys().size(), 1);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(result.keys()[0].value(), varref(cr0));

    EXPECT_EQ(result.condition(), nullptr);
}

TEST_F(rewrite_join_test, right_indirect) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& filter = r.insert(relation::filter {
            compare(varref(cr1), constant(1)),
    });

    inl.output() >> join.left();
    inr.output() >> filter.input();
    filter.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[1],
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 3);

    auto&& result = next<relation::join_find>(out.input());
    EXPECT_GT(inl.output(), result.left());

    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 2);
    EXPECT_EQ(result.columns()[0].source(), bindings(t1c0));
    EXPECT_EQ(result.columns()[1].source(), bindings(t1c1));
    EXPECT_EQ(result.columns()[0].destination(), cr0);
    EXPECT_EQ(result.columns()[1].destination(), cr1);

    ASSERT_EQ(result.keys().size(), 2);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t1c1));
    EXPECT_EQ(result.keys()[1].variable(), bindings(t1c0));
    EXPECT_EQ(result.keys()[0].value(), constant(1));
    EXPECT_EQ(result.keys()[1].value(), varref(cl0));

    EXPECT_EQ(result.condition(), nullptr);
}

TEST_F(rewrite_join_test, left_indirect) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& filter = r.insert(relation::filter {
            compare(varref(cl1), constant(1)),
    });
    inl.output() >> filter.input();
    filter.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t0,
            "x0",
            {
                    t0->columns()[1],
                    t0->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 3);

    auto&& result = next<relation::join_find>(out.input());
    EXPECT_GT(inr.output(), result.left());

    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 2);
    EXPECT_EQ(result.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(result.columns()[1].source(), bindings(t0c1));
    EXPECT_EQ(result.columns()[0].destination(), cl0);
    EXPECT_EQ(result.columns()[1].destination(), cl1);

    ASSERT_EQ(result.keys().size(), 2);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t0c1));
    EXPECT_EQ(result.keys()[1].variable(), bindings(t0c0));
    EXPECT_EQ(result.keys()[0].value(), constant(1));
    EXPECT_EQ(result.keys()[1].value(), varref(cr0));

    EXPECT_EQ(result.condition(), nullptr);
}

TEST_F(rewrite_join_test, right_interfare) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& interfare = r.insert(relation::intermediate::distinct {
            { cr0 },
    });

    inl.output() >> join.left();
    inr.output() >> interfare.input();
    interfare.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 5);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_GT(interfare.output(), result.right());
}

TEST_F(rewrite_join_test, left_interfare) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& interfare = r.insert(relation::intermediate::distinct {
            { cl0 },
    });

    inl.output() >> interfare.input();
    interfare.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t0,
            "x0",
            {
                    t0->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 5);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_GT(interfare.output(), result.left());
}

TEST_F(rewrite_join_test, right_direct_outer) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 3);

    auto&& result = next<relation::join_find>(out.input());
    EXPECT_GT(inl.output(), result.left());

    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 1);
    EXPECT_EQ(result.columns()[0].source(), bindings(t1c0));
    EXPECT_EQ(result.columns()[0].destination(), cr0);

    ASSERT_EQ(result.keys().size(), 1);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t1c0));
    EXPECT_EQ(result.keys()[0].value(), varref(cl0));

    EXPECT_EQ(result.condition(), nullptr);
}

TEST_F(rewrite_join_test, left_direct_outer) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t0,
            "x0",
            {
                    t0->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 4);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_EQ(result.condition(), compare(cl0, cr0));
}

TEST_F(rewrite_join_test, right_indirect_outer) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& filter = r.insert(relation::filter {
            compare(varref(cr1), constant(1)),
    });

    inl.output() >> join.left();
    inr.output() >> filter.input();
    filter.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[1],
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 5);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_EQ(result.condition(), compare(cl0, cr0));
    EXPECT_EQ(filter.condition(), compare(varref(cr1), constant(1)));
}

TEST_F(rewrite_join_test, left_indirect_outer) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& filter = r.insert(relation::filter {
            compare(varref(cl1), constant(1)),
    });
    inl.output() >> filter.input();
    filter.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t0,
            "x0",
            {
                    t0->columns()[1],
                    t0->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 5);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_EQ(result.condition(), compare(cl0, cr0));
    EXPECT_EQ(filter.condition(), compare(varref(cl1), constant(1)));
}

TEST_F(rewrite_join_test, join_scan) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            land(compare(constant(0), varref(cr0), comparison_operator::less),
                    compare(cr0, cl0, comparison_operator::less_equal)),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 3);

    auto&& result = next<relation::join_scan>(out.input());
    EXPECT_GT(inl.output(), result.left());

    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 1);
    EXPECT_EQ(result.columns()[0].source(), bindings(t1c0));
    EXPECT_EQ(result.columns()[0].destination(), cr0);

    ASSERT_EQ(result.lower().keys().size(), 1);
    EXPECT_EQ(result.lower().keys()[0].variable(), bindings(t1c0));
    EXPECT_EQ(result.lower().keys()[0].value(), constant(0));
    ASSERT_EQ(result.lower().kind(), relation::endpoint_kind::prefixed_exclusive);

    ASSERT_EQ(result.upper().keys().size(), 1);
    EXPECT_EQ(result.upper().keys()[0].variable(), bindings(t1c0));
    EXPECT_EQ(result.upper().keys()[0].value(), varref(cl0));
    ASSERT_EQ(result.upper().kind(), relation::endpoint_kind::prefixed_inclusive);

    EXPECT_EQ(result.condition(), land(boolean(true), boolean(true)));
}

TEST_F(rewrite_join_test, join_scan_suppressed) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            land(compare(constant(0), varref(cr0), comparison_operator::less),
                    compare(cr0, cl0, comparison_operator::less_equal)),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[0],
            },
    });
    apply_custom(r, false);

    ASSERT_EQ(r.size(), 4);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_EQ(result.condition(), land(compare(constant(0), varref(cr0), comparison_operator::less),
            compare(cr0, cl0, comparison_operator::less_equal)));
}

TEST_F(rewrite_join_test, indirect_rest) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& filter = r.insert(relation::filter {
            compare(varref(cr1), constant(1)),
    });

    inl.output() >> join.left();
    inr.output() >> filter.input();
    filter.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 3);

    auto&& result = next<relation::join_find>(out.input());
    EXPECT_GT(inl.output(), result.left());

    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 2);
    EXPECT_EQ(result.columns()[0].source(), bindings(t1c0));
    EXPECT_EQ(result.columns()[1].source(), bindings(t1c1));
    EXPECT_EQ(result.columns()[0].destination(), cr0);
    EXPECT_EQ(result.columns()[1].destination(), cr1);

    ASSERT_EQ(result.keys().size(), 1);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t1c0));
    EXPECT_EQ(result.keys()[0].value(), varref(cl0));

    EXPECT_EQ(result.condition(), compare(varref(cr1), constant(1)));
}

TEST_F(rewrite_join_test, right_cross_indirect) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c1), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });

    auto&& filter = r.insert(relation::filter {
            compare(varref(cr1), constant(1)),
    });

    inl.output() >> join.left();
    inr.output() >> filter.input();
    filter.output() >> join.right();
    join.output() >> out.input();

    auto x0 = storages.add_index(storage::index {
            t1,
            "x0",
            {
                    t1->columns()[1],
                    t1->columns()[0],
            },
    });
    apply(r);

    ASSERT_EQ(r.size(), 5);

    auto&& result = next<relation::intermediate::join>(out.input());
    EXPECT_EQ(result.condition(), nullptr);
}

TEST_F(rewrite_join_test, fix_heap_reuse) {
    auto t_c = storages.add_table({
            "customer",
            {
                    { "c_id", t::int4()} ,
                    { "c_w_id", t::int4() },
                    { "c_d_id", t::int4() },
                    { "c_x", t::int4() },
            },
    });
    auto t_w = storages.add_table({
            "warehouse",
            {
                    { "w_id", t::int4()} ,
                    { "w_x", t::int4()} ,
            },
    });
    auto i_c = storages.add_index({
            t_c,
            "customer_primary",
            {
                    t_c->columns()[0],
                    t_c->columns()[1],
                    t_c->columns()[2],
            },
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });
    auto i_w = storages.add_index({
            t_w,
            "warehouse_primary",
            {
                    t_w->columns()[0],
            },
            {},
            {
                    ::yugawara::storage::index_feature::find,
                    ::yugawara::storage::index_feature::scan,
                    ::yugawara::storage::index_feature::unique,
                    ::yugawara::storage::index_feature::primary,
            },
    });

    /*
    "SELECT w_x, c_x FROM warehouse, customer "
    "WHERE w_id = :w_id "
    "AND c_w_id = w_id AND "
    "c_d_id = :c_d_id AND "
    "c_id = :c_id "
    */
    relation::graph_type r;

    auto w_id = bindings.stream_variable("w_id");
    auto w_x = bindings.stream_variable("w_x");
    auto&& inl = r.insert(relation::scan {
            bindings(*i_w),
            {
                    { bindings(t_w->columns()[0]), w_id },
                    { bindings(t_w->columns()[1]), w_x },
            },
    });

    auto c_id = bindings.stream_variable("c_id");
    auto c_w_id = bindings.stream_variable("c_w_id");
    auto c_d_id = bindings.stream_variable("c_d_id");
    auto c_x = bindings.stream_variable("c_x");
    auto&& inr = r.insert(relation::scan {
            bindings(*i_c),
            {
                    { bindings(t_c->columns()[0]), c_id },
                    { bindings(t_c->columns()[1]), c_w_id },
                    { bindings(t_c->columns()[2]), c_d_id },
                    { bindings(t_c->columns()[3]), c_x },
            },
    });

    /*
    "SELECT w_x, c_x FROM warehouse, customer "
    "WHERE w_id = :w_id "
    "AND c_w_id = w_id AND "
    "c_d_id = :c_d_id AND "
    "c_id = :c_id "
    */

    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });

    auto&& filter = r.insert(relation::filter {
            land(
                    compare(varref(w_id), constant(1)),
                    land(
                            compare(varref(c_w_id), varref(w_id)),
                            land(
                                    compare(varref(c_d_id), constant(2)),
                                    compare(varref(c_id), constant(3))))),
    });

    auto&& out = r.insert(relation::emit { w_x, c_x });

    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> filter.input();
    filter.output() >> out.input();

    apply(r);
}


} // namespace yugawara::analyzer::details
