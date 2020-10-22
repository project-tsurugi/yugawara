#include <yugawara/analyzer/details/collect_exchange_steps.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>

#include <takatori/relation/step/join.h>
#include <takatori/relation/step/aggregate.h>
#include <takatori/relation/step/intersection.h>
#include <takatori/relation/step/difference.h>

#include <takatori/plan/graph.h>
#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>
#include <takatori/plan/group.h>
#include <takatori/plan/aggregate.h>
#include <takatori/plan/broadcast.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class collect_exchange_steps_test : public ::testing::Test {
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
};

TEST_F(collect_exchange_steps_test, simple) {
    /*
     * scan:r0 - emit:r1
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - emit:r1
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 2);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_LT(r1.input(), r0.output());

    ASSERT_EQ(p.size(), 0);
}

TEST_F(collect_exchange_steps_test, join_default) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, c1 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto& r3 = r.insert(relation::emit {
            c0,
            c1,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r4 - [group]:e0 -\
     *                                   take_cogroup:r6 - join_group:r7 - emit:r3
     * scan:r1 - offer:r5 - [group]:e1 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 7);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r7 = next<relation::step::join>(r3.input());
    auto&& r6 = next<take_cogroup>(r7.input());

    auto&& e0 = resolve<plan::group>(r4.destination());
    auto&& e1 = resolve<plan::group>(r5.destination());

    ASSERT_EQ(p.size(), 2);
    EXPECT_TRUE(p.contains(e0));
    EXPECT_TRUE(p.contains(e1));

    ASSERT_EQ(r6.groups().size(), 2);
    EXPECT_EQ(r6.groups()[0].source(), r4.destination());
    EXPECT_EQ(r6.groups()[1].source(), r5.destination());

    ASSERT_EQ(e0.group_keys().size(), 0);
    EXPECT_EQ(e0.limit(), std::nullopt);

    ASSERT_EQ(e1.group_keys().size(), 0);
    EXPECT_EQ(e1.limit(), std::nullopt);

    EXPECT_EQ(r7.condition(), nullptr);
}

TEST_F(collect_exchange_steps_test, join_cogroup) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c1");
    auto c3 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    r2.lower() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    auto& r3 = r.insert(relation::emit {
            c1,
            c3,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    options.add(r2, join_strategy::cogroup);
    plan::graph_type p;

    /*
     * scan:r0 - offer:r4 - [group]:e0 -\
     *                                   take_cogroup:r6 - join_group:r7 - emit:r3
     * scan:r1 - offer:r5 - [group]:e1 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 7);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r7 = next<relation::step::join>(r3.input());
    auto&& r6 = next<take_cogroup>(r7.input());

    auto&& e0 = resolve<plan::group>(r4.destination());
    auto&& e1 = resolve<plan::group>(r5.destination());

    ASSERT_EQ(p.size(), 2);
    EXPECT_TRUE(p.contains(e0));
    EXPECT_TRUE(p.contains(e1));

    ASSERT_EQ(r6.groups().size(), 2);
    EXPECT_EQ(r6.groups()[0].source(), r4.destination());
    EXPECT_EQ(r6.groups()[1].source(), r5.destination());

    ASSERT_EQ(e0.group_keys().size(), 1);
    ASSERT_EQ(e0.group_keys()[0], c0);
    EXPECT_EQ(e0.limit(), std::nullopt);

    ASSERT_EQ(e1.group_keys().size(), 1);
    ASSERT_EQ(e1.group_keys()[0], c2);
    EXPECT_EQ(e1.limit(), std::nullopt);

    EXPECT_EQ(r7.condition(), nullptr);
}

TEST_F(collect_exchange_steps_test, join_cogroup_default) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c1");
    auto c3 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    r2.lower() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    auto& r3 = r.insert(relation::emit {
            c1,
            c3,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r4 - [group]:e0 -\
     *                                   take_cogroup:r6 - join_group:r7 - emit:r3
     * scan:r1 - offer:r5 - [group]:e1 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 7);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r7 = next<relation::step::join>(r3.input());
    auto&& r6 = next<take_cogroup>(r7.input());

    auto&& e0 = resolve<plan::group>(r4.destination());
    auto&& e1 = resolve<plan::group>(r5.destination());

    ASSERT_EQ(p.size(), 2);
    EXPECT_TRUE(p.contains(e0));
    EXPECT_TRUE(p.contains(e1));

    ASSERT_EQ(r6.groups().size(), 2);
    EXPECT_EQ(r6.groups()[0].source(), r4.destination());
    EXPECT_EQ(r6.groups()[1].source(), r5.destination());

    ASSERT_EQ(e0.group_keys().size(), 1);
    ASSERT_EQ(e0.group_keys()[0], c0);
    EXPECT_EQ(e0.limit(), std::nullopt);

    ASSERT_EQ(e1.group_keys().size(), 1);
    ASSERT_EQ(e1.group_keys()[0], c2);
    EXPECT_EQ(e1.limit(), std::nullopt);

    EXPECT_EQ(r7.condition(), nullptr);
}

TEST_F(collect_exchange_steps_test, join_broadcast_find) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    r2.lower() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    auto& r3 = r.insert(relation::emit {
            c1,
            c3,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    options.add(r2, join_strategy::broadcast);
    plan::graph_type p;

    /*
     *                              scan:r0 - join_find:r4 - emit:r3
     *                                       /
     * scan:r1 - offer:r5 - [broadcast]:e0 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<relation::join_find>(r0.output());
    auto&& r5 = next<offer>(r1.output());

    auto&& e0 = resolve<plan::broadcast>(r5.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r5.destination());
    ASSERT_EQ(r4.keys().size(), 1);
    EXPECT_EQ(r4.keys()[0].variable(), c2);
    EXPECT_EQ(r4.keys()[0].value(), varref(c0));
}

TEST_F(collect_exchange_steps_test, join_broadcast_find_default) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    r2.lower() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            constant(100),
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            constant(100),
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    auto& r3 = r.insert(relation::emit {
            c1,
            c3,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     *                              scan:r0 - join_find:r4 - emit:r3
     *                                       /
     * scan:r1 - offer:r5 - [broadcast]:e0 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<relation::join_find>(r0.output());
    auto&& r5 = next<offer>(r1.output());

    auto&& e0 = resolve<plan::broadcast>(r5.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r5.destination());
    ASSERT_EQ(r4.keys().size(), 1);
    EXPECT_EQ(r4.keys()[0].variable(), c2);
    EXPECT_EQ(r4.keys()[0].value(), constant(100));
}

TEST_F(collect_exchange_steps_test, join_broadcast_scan) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c1");
    auto c3 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    r2.lower() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            constant(0),
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_exclusive,
    };
    auto& r3 = r.insert(relation::emit {
            c1,
            c3,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    options.add(r2, join_strategy::broadcast);
    plan::graph_type p;

    /*
     *                              scan:r0 - join_scan:r4 - emit:r3
     *                                       /
     * scan:r1 - offer:r5 - [broadcast]:e0 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<relation::join_scan>(r0.output());
    auto&& r5 = next<offer>(r1.output());

    auto&& e0 = resolve<plan::broadcast>(r5.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r5.destination());
    ASSERT_EQ(r4.lower().keys().size(), 1);
    EXPECT_EQ(r4.lower().keys()[0].variable(), c2);
    EXPECT_EQ(r4.lower().keys()[0].value(), constant(0));
    EXPECT_EQ(r4.lower().kind(), relation::endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(r4.upper().keys().size(), 1);
    EXPECT_EQ(r4.upper().keys()[0].variable(), c2);
    EXPECT_EQ(r4.upper().keys()[0].value(), varref(c0));
    EXPECT_EQ(r4.upper().kind(), relation::endpoint_kind::prefixed_exclusive);
}

TEST_F(collect_exchange_steps_test, join_broadcast_scan_default) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c1");
    auto c3 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    r2.lower() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            constant(0),
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                    },
            },
            relation::endpoint_kind::prefixed_exclusive,
    };
    auto& r3 = r.insert(relation::emit {
            c1,
            c3,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     *                              scan:r0 - join_scan:r4 - emit:r3
     *                                       /
     * scan:r1 - offer:r5 - [broadcast]:e0 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<relation::join_scan>(r0.output());
    auto&& r5 = next<offer>(r1.output());

    auto&& e0 = resolve<plan::broadcast>(r5.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r5.destination());
    ASSERT_EQ(r4.lower().keys().size(), 1);
    EXPECT_EQ(r4.lower().keys()[0].variable(), c2);
    EXPECT_EQ(r4.lower().keys()[0].value(), constant(0));
    EXPECT_EQ(r4.lower().kind(), relation::endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(r4.upper().keys().size(), 1);
    EXPECT_EQ(r4.upper().keys()[0].variable(), c2);
    EXPECT_EQ(r4.upper().keys()[0].value(), varref(c0));
    EXPECT_EQ(r4.upper().kind(), relation::endpoint_kind::prefixed_exclusive);
}

TEST_F(collect_exchange_steps_test, aggregate_default_group) {
    aggregate::configurable_provider aggregates;
    auto func = aggregates.add({
            aggregate::declaration::minimum_builtin_function_id + 1,
            "testing",
            t::int4 {},
            {
                    t::int4 {},
            },
            false,
    });

    /*
     * scan:r0 - aggregate_relation:r1 - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    { bindings(func), c1, c2, },
            },
    });
    auto& r2 = r.insert(relation::emit {
            c0,
            c2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [group] - take_group:r4 - aggregate_group:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r2));

    auto&& r3 = next<offer>(r0.output());
    auto&& r5 = next<relation::step::aggregate>(r2.input());
    auto&& r4 = next<take_group>(r5.input());

    auto&& e0 = resolve<plan::group>(r3.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r3.destination());

    ASSERT_EQ(e0.group_keys().size(), 1);
    EXPECT_EQ(e0.group_keys()[0], c0);
    EXPECT_EQ(e0.limit(), std::nullopt);

    ASSERT_EQ(r5.columns().size(), 1);
    EXPECT_EQ(r5.columns()[0].function(), bindings(func));
    ASSERT_EQ(r5.columns()[0].arguments().size(), 1);
    EXPECT_EQ(r5.columns()[0].arguments()[0], c1);
    EXPECT_EQ(r5.columns()[0].destination(), c2);
}

TEST_F(collect_exchange_steps_test, aggregate_default_exchange) {
    aggregate::configurable_provider aggregates;
    auto func = aggregates.add({
            aggregate::declaration::minimum_builtin_function_id + 2,
            "testing",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });

    /*
     * scan:r0 - aggregate_relation:r1 - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    { bindings(func), c1, c2, },
            },
    });
    auto& r2 = r.insert(relation::emit {
            c0,
            c2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [aggregate] - take_group:r4 - flatten_group:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r2));

    auto&& r3 = next<offer>(r0.output());
    auto&& r5 = next<relation::step::flatten>(r2.input());
    auto&& r4 = next<take_group>(r5.input());

    auto&& e0 = resolve<plan::aggregate>(r3.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r3.destination());

    ASSERT_EQ(e0.group_keys().size(), 1);
    EXPECT_EQ(e0.group_keys()[0], c0);

    ASSERT_EQ(e0.aggregations().size(), 1);
    EXPECT_EQ(e0.aggregations()[0].function(), bindings(func));
    ASSERT_EQ(e0.aggregations()[0].arguments().size(), 1);
    EXPECT_EQ(e0.aggregations()[0].arguments()[0], c1);
    EXPECT_EQ(e0.aggregations()[0].destination(), c2);
}

TEST_F(collect_exchange_steps_test, aggregate_exchange_disabled) {
    aggregate::configurable_provider aggregates;
    auto func = aggregates.add({
            aggregate::declaration::minimum_builtin_function_id + 2,
            "testing",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });

    /*
     * scan:r0 - aggregate_relation:r1 - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    { bindings(func), c1, c2, },
            },
    });
    auto& r2 = r.insert(relation::emit {
            c0,
            c2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    options.runtime_features().erase(runtime_feature::aggregate_exchange);
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [group] - take_group:r4 - aggregate_group:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r2));

    auto&& r3 = next<offer>(r0.output());
    auto&& r5 = next<relation::step::aggregate>(r2.input());
    auto&& r4 = next<take_group>(r5.input());

    auto&& e0 = resolve<plan::group>(r3.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r3.destination());

    ASSERT_EQ(e0.group_keys().size(), 1);
    EXPECT_EQ(e0.group_keys()[0], c0);
    EXPECT_EQ(e0.limit(), std::nullopt);

    ASSERT_EQ(r5.columns().size(), 1);
    EXPECT_EQ(r5.columns()[0].function(), bindings(func));
    ASSERT_EQ(r5.columns()[0].arguments().size(), 1);
    EXPECT_EQ(r5.columns()[0].arguments()[0], c1);
    EXPECT_EQ(r5.columns()[0].destination(), c2);
}

TEST_F(collect_exchange_steps_test, distinct) {
    /*
     * scan:r0 - distinct_relation:r1 - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::distinct {
            c0,
            c1,
    });
    auto& r2 = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [group] - take_group:r4 - flattern:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r2));

    auto&& r3 = next<offer>(r0.output());
    auto&& r5 = next<flatten>(r2.input());
    auto&& r4 = next<take_group>(r5.input());

    auto&& e0 = resolve<plan::group>(r3.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r3.destination());

    ASSERT_EQ(e0.group_keys().size(), 2);
    EXPECT_EQ(e0.group_keys()[0], c0);
    EXPECT_EQ(e0.group_keys()[1], c1);
    ASSERT_EQ(e0.limit(), 1);
}

TEST_F(collect_exchange_steps_test, limit_flat) {
    /*
     * scan:r0 - limit_relation:r1 - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::limit {
            10,
    });
    auto& r2 = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [forward] - take_flat:r4 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 4);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r2));

    auto&& r3 = next<offer>(r0.output());
    auto&& r4 = next<relation::step::take_flat>(r2.input());

    auto&& e0 = resolve<plan::forward>(r3.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r3.destination());

    ASSERT_EQ(e0.limit(), 10);
}

TEST_F(collect_exchange_steps_test, limit_group) {
    /*
     * scan:r0 - distinct_relation:r1 - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::limit {
            10,
            {
                    c0,
                    c1,
            },
    });
    auto& r2 = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [group] - take_group:r4 - flattern:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r2));

    auto&& r3 = next<offer>(r0.output());
    auto&& r5 = next<flatten>(r2.input());
    auto&& r4 = next<take_group>(r5.input());

    auto&& e0 = resolve<plan::group>(r3.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r3.destination());

    ASSERT_EQ(e0.group_keys().size(), 2);
    EXPECT_EQ(e0.group_keys()[0], c0);
    EXPECT_EQ(e0.group_keys()[1], c1);
    ASSERT_EQ(e0.limit(), 10);
}

TEST_F(collect_exchange_steps_test, union_all) {
    /*
     * scan:r0 -\
     *           union:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto co0 = bindings.stream_variable("co0");
    auto co1 = bindings.stream_variable("co1");
    auto co2 = bindings.stream_variable("co2");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c0, cl1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::union_ {
            {
                    { cl0, cr0, co0, },
                    { cl1,  {}, co1, },
                    {  {}, cr1, co2, },
            },
            relation::set_quantifier::all,
    });
    auto& r3 = r.insert(relation::emit {
            co0,
            co1,
            co2,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r4 -\
     *                      [forward]:e0 - take_flat:r6 - emit:r3
     * scan:r1 - offer:r5 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 6);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r6 = next<take_flat>(r3.input());

    auto&& e0 = resolve<plan::forward>(r4.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.destination(), r6.source());
    ASSERT_EQ(r4.columns().size(), 2);
    EXPECT_EQ(r4.columns()[0].source(), cl0);
    EXPECT_EQ(r4.columns()[0].destination(), co0);
    EXPECT_EQ(r4.columns()[1].source(), cl1);
    EXPECT_EQ(r4.columns()[1].destination(), co1);

    EXPECT_EQ(r5.destination(), r6.source());
    ASSERT_EQ(r5.columns().size(), 2);
    EXPECT_EQ(r5.columns()[0].source(), cr0);
    EXPECT_EQ(r5.columns()[0].destination(), co0);
    EXPECT_EQ(r5.columns()[1].source(), cr1);
    EXPECT_EQ(r5.columns()[1].destination(), co2);

    ASSERT_EQ(e0.columns().size(), 3);
    EXPECT_EQ(e0.columns()[0], co0);
    EXPECT_EQ(e0.columns()[1], co1);
    EXPECT_EQ(e0.columns()[2], co2);
    EXPECT_EQ(e0.limit(), std::nullopt);

    ASSERT_EQ(r6.columns().size(), 0);
}

TEST_F(collect_exchange_steps_test, union_distinct) {
    /*
     * scan:r0 -\
     *           union:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto co0 = bindings.stream_variable("co0");
    auto co1 = bindings.stream_variable("co1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c0, cl1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::union_ {
            {
                    { cl0, cr0, co0, },
                    { cl1, cr1, co1, },
            },
            relation::set_quantifier::distinct,
    });
    auto& r3 = r.insert(relation::emit {
            co0,
            co1,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r4 -\
     *                      [group]:e0 - take_group:r6 - flatten:r7 - emit:r3
     * scan:r1 - offer:r5 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 7);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r7 = next<flatten>(r3.input());
    auto&& r6 = next<take_group>(r7.input());

    auto&& e0 = resolve<plan::group>(r4.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.destination(), r6.source());
    ASSERT_EQ(r4.columns().size(), 2);
    EXPECT_EQ(r4.columns()[0].source(), cl0);
    EXPECT_EQ(r4.columns()[0].destination(), co0);
    EXPECT_EQ(r4.columns()[1].source(), cl1);
    EXPECT_EQ(r4.columns()[1].destination(), co1);

    EXPECT_EQ(r5.destination(), r6.source());
    ASSERT_EQ(r5.columns().size(), 2);
    EXPECT_EQ(r5.columns()[0].source(), cr0);
    EXPECT_EQ(r5.columns()[0].destination(), co0);
    EXPECT_EQ(r5.columns()[1].source(), cr1);
    EXPECT_EQ(r5.columns()[1].destination(), co1);

    ASSERT_EQ(e0.columns().size(), 2);
    EXPECT_EQ(e0.group_keys()[0], co0);
    EXPECT_EQ(e0.group_keys()[1], co1);
    EXPECT_EQ(e0.columns()[0], co0);
    EXPECT_EQ(e0.columns()[1], co1);
    EXPECT_EQ(e0.limit(), 1);

    ASSERT_EQ(r6.columns().size(), 0);
}

TEST_F(collect_exchange_steps_test, binary_all) {
    /*
     * scan:r0 -\
     *           intersection_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c0, cl1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::intersection {
            {
                    { cl0, cr0, },
                    { cl1, cr1, },
            },
            relation::set_quantifier::all,
    });
    auto& r3 = r.insert(relation::emit {
            cl0,
            cl1,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r4 - [group]:e0 -\
     *                                   take_cogroup:r6 - intersection_group:r7 - emit:r3
     * scan:r1 - offer:r5 - [group]:e1 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 7);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r7 = next<relation::step::intersection>(r3.input());
    auto&& r6 = next<take_cogroup>(r7.input());

    auto&& e0 = resolve<plan::group>(r4.destination());
    auto&& e1 = resolve<plan::group>(r5.destination());

    ASSERT_EQ(p.size(), 2);
    EXPECT_TRUE(p.contains(e0));
    EXPECT_TRUE(p.contains(e1));

    ASSERT_EQ(r6.groups().size(), 2);
    EXPECT_EQ(r6.groups()[0].source(), r4.destination());
    EXPECT_EQ(r6.groups()[1].source(), r5.destination());

    ASSERT_EQ(e0.group_keys().size(), 2);
    EXPECT_EQ(e0.group_keys()[0], cl0);
    EXPECT_EQ(e0.group_keys()[1], cl1);
    EXPECT_EQ(e0.limit(), std::nullopt);

    ASSERT_EQ(e1.group_keys().size(), 2);
    EXPECT_EQ(e1.group_keys()[0], cr0);
    EXPECT_EQ(e1.group_keys()[1], cr1);
    EXPECT_EQ(e1.limit(), std::nullopt);
}

TEST_F(collect_exchange_steps_test, binary_distinct) {
    /*
     * scan:r0 -\
     *           difference_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c0, cl1 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::difference {
            {
                    { cl0, cr0, },
                    { cl1, cr1, },
            },
            relation::set_quantifier::distinct,
    });
    auto& r3 = r.insert(relation::emit {
            cl0,
            cl1,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r4 - [group]:e0 -\
     *                                   take_cogroup:r6 - difference_group:r7 - emit:r3
     * scan:r1 - offer:r5 - [group]:e1 -/
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 7);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r7 = next<relation::step::difference>(r3.input());
    auto&& r6 = next<take_cogroup>(r7.input());

    auto&& e0 = resolve<plan::group>(r4.destination());
    auto&& e1 = resolve<plan::group>(r5.destination());

    ASSERT_EQ(p.size(), 2);
    EXPECT_TRUE(p.contains(e0));
    EXPECT_TRUE(p.contains(e1));

    ASSERT_EQ(r6.groups().size(), 2);
    EXPECT_EQ(r6.groups()[0].source(), r4.destination());
    EXPECT_EQ(r6.groups()[1].source(), r5.destination());

    ASSERT_EQ(e0.group_keys().size(), 2);
    EXPECT_EQ(e0.group_keys()[0], cl0);
    EXPECT_EQ(e0.group_keys()[1], cl1);
    EXPECT_EQ(e0.limit(), 1);

    ASSERT_EQ(e1.group_keys().size(), 2);
    EXPECT_EQ(e1.group_keys()[0], cr0);
    EXPECT_EQ(e1.group_keys()[1], cr1);
    EXPECT_EQ(e1.limit(), 1);
}

} // namespace yugawara::analyzer::details
