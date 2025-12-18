#include <yugawara/analyzer/details/push_down_filters.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/identify.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>

#include <takatori/util/downcast.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::util::downcast;

class push_down_filters_test : public ::testing::Test {
protected:
    type::repository types;
    binding::factory bindings;

    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::table> t1 = storages.add_table({
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    descriptor::variable t0c0 = bindings(t0->columns()[0]);
    descriptor::variable t0c1 = bindings(t0->columns()[1]);
    descriptor::variable t0c2 = bindings(t0->columns()[2]);
    descriptor::variable t1c0 = bindings(t1->columns()[0]);
    descriptor::variable t1c1 = bindings(t1->columns()[1]);
    descriptor::variable t1c2 = bindings(t1->columns()[2]);

    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages.add_index({ t1, "I1" });

    aggregate::configurable_provider aggregates;
    std::shared_ptr<aggregate::declaration> agg0 = aggregates.add(aggregate::declaration {
            aggregate::declaration::minimum_builtin_function_id + 1,
            "agg0",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });
    std::shared_ptr<aggregate::declaration> agg1 = aggregates.add(aggregate::declaration {
            agg0->definition_id() + 1,
            "agg1",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });

    static relation::expression::input_port_type& connect(relation::expression::output_port_type& output) {
        if (output.opposite()) {
            throw std::domain_error("must not be connected");
        }

        auto&& r = output.owner().owner();

        /*
         * .. - emit
         */
        auto&& emit = r.insert(relation::emit {});
        return emit.input() << output;
    }

    void apply(relation::graph_type& r) {
        push_down_selections(r);
    }
};

TEST_F(push_down_filters_test, find) {
    /*
     * scan:r0 - filter:rf - ..
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::find {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
            {
                    relation::find::key { t0c0, constant() },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(c0, c1));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, scan) {
    /*
     * scan:r0 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(c0, c1));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, values) {
    /*
     * values:r0 - filter:rf - ..
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(c0, c1));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_inner_over_left) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_FALSE(rj.condition());
}

TEST_F(push_down_filters_test, join_relation_inner_over_right) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_FALSE(rj.condition());
}

TEST_F(push_down_filters_test, join_relation_inner_over_both) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(constant(0), constant(0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto&& f0 = next<relation::filter>(rl.output());
    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_GT(f1.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(constant(0), constant(0)));
    EXPECT_EQ(f1.condition(), compare(constant(0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_FALSE(rj.condition());
}

TEST_F(push_down_filters_test, join_relation_inner_to_condition) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), varref(cr0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);

    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr0)));
}

TEST_F(push_down_filters_test, join_relation_inner_to_left) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(cl0), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));
    EXPECT_EQ(rj.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_inner_to_right) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(cr0), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));
    EXPECT_EQ(rj.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_inner_to_both) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(constant(0), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_GT(f1.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(constant(0), constant(1)));
    EXPECT_EQ(f1.condition(), compare(constant(0), constant(1)));
    EXPECT_EQ(rj.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_inner_keep_condition) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(cl0), varref(cr0)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr0)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_over_left) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(l0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_flush_right) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(r0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rj.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_full_outer_flush_left) {
    /*
     * scan:rl -\
     *           join_relation(full on:l0=r0):rj - filter(l0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rj.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_full_outer_flush_right) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(r0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rj.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, apply_over_left) {
    /*
     * scan:r0 - apply:r1 - filter:rf - ...
     * filter only refers left columns
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "o1", ::takatori::type::int8 {} },
                    { "o2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { cl1 }
            },
            {
                    cr0,
                    cr1,
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, apply_flush_use_right) {
    /*
     * scan:r0 - apply:r1 - filter:rf - ...
     * filter refers right columns
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "o1", ::takatori::type::int8 {} },
                    { "o2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { cl1 }
            },
            {
                    cr0,
                    cr1,
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, apply_flush_use_left_right) {
    /*
     * scan:r0 - apply:r1 - filter:rf - ...
     * filter refers left and right columns
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "o1", ::takatori::type::int8 {} },
                    { "o2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { cl1 }
            },
            {
                    cr0,
                    cr1,
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), varref(cr0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), varref(cr0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, project_flush) {
    /*
     * scan:r0 - project:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c0 },
                            constant(1),
                    },
                    x0,
            },
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c1 },
                            constant(2),
                    },
                    x1,
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x1), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x1), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, project_pass) {
    /*
     * scan:r0 - project:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c0 },
                            constant(1),
                    },
                    x0,
            },
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c1 },
                            constant(2),
                    },
                    x1,
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, buffer) {
    /*
     *                    /- filter:rf - ...
     * scan:r0 - buffer:r1
     *                    \- ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = r.insert(relation::buffer { 2 });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output_ports()[0] >> rf.input();

    connect(rf.output());
    connect(r1.output_ports()[1]);
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r1.output_ports()[0]);
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, identify_flush) {
    /*
     * scan:r0 - identify:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::identify {
            x0,
            t::row_id { 2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x0), varref(x0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x0), varref(x0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, identify_pass) {
    /*
     * scan:r0 - identify:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::identify {
            x0,
            t::row_id { 2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, aggregate_relation_flush) {
    /*
     * scan:r0 - aggregate_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x1 = bindings.stream_variable("c3");
    auto x2 = bindings.stream_variable("c3");
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    {
                            bindings(agg0),
                            { c1 },
                            x1,
                    },
                    {
                            bindings(agg1),
                            { c2 },
                            x2,
                    },

            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x1), varref(c0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x1), varref(c0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, aggregate_relation_through) {
    /*
     * scan:r0 - aggregate_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x1 = bindings.stream_variable("c3");
    auto x2 = bindings.stream_variable("c3");
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    {
                            bindings(agg0),
                            { c1 },
                            x1,
                    },
                    {
                            bindings(agg1),
                            { c2 },
                            x2,
                    },

            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, distinct_relation_flush) {
    /*
     * scan:r0 - distinct_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::distinct {
            c0,
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), varref(c0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    auto&& f1 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), r1.input());
    EXPECT_GT(f1.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), varref(c0)));
    EXPECT_EQ(f1.condition(), compare(varref(c2), varref(c0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, distinct_relation_pass) {
    /*
     * scan:r0 - distinct_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::distinct {
            c0,
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, limit_relation_flush) {
    /*
     * scan:r0 - limit_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::limit {
            {},
            { c0, },
            { c1, },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), varref(c0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    auto&& f1 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), r1.input());
    EXPECT_GT(f1.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), varref(c0)));
    EXPECT_EQ(f1.condition(), compare(varref(c2), varref(c0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, limit_relation_pass) {
    /*
     * scan:r0 - limit_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::limit {
            {},
            { c0, },
            { c1, },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

// FIXME: impl other than union both

TEST_F(push_down_filters_test, union_relation_both) {
    /*
     * scan:rl -\
     *           union_relation:r0 - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto&& r0 = r.insert(relation::intermediate::union_ {
            { cl0, cl0, x0 },
            { cl1,  {}, x1 },
            {  {}, cl1, x2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x1), varref(x2)),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x1), varref(x2)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, intersection_relation) {
    /*
     * scan:rl -\
     *           intersection_relation:r0 - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto&& r0 = r.insert(relation::intermediate::intersection {
            { cl0, cr0 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl1), constant(0)),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), r0.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, difference_relation) {
    /*
     * scan:rl -\
     *           difference_relation:r0 - filter:rd - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto&& r0 = r.insert(relation::intermediate::difference {
            { cl0, cr0 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl1), constant(0)),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), r0.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, escape) {
    /*
     * scan:r0 - escape:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto&& r1 = r.insert(relation::intermediate::escape {
            { c0, x0 },
            { c1, x1 },
            { c2, x2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

} // namespace yugawara::analyzer::details
