#include <yugawara/analyzer/step_plan_builder.h>

#include <gtest/gtest.h>

#include <takatori/type/int.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/escape.h>

#include <takatori/relation/step/join.h>
#include <takatori/relation/step/take_cogroup.h>
#include <takatori/relation/step/offer.h>

#include <takatori/plan/group.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include "details/utils.h"

namespace yugawara::analyzer {

// import test utils
using namespace details;

/*
 * NOTE: in this fixture, we only tests simple scenarios.
 * ./details dir contains more detailed fixtures.
 */

class step_plan_builder_test : public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    type::repository types;
    binding::factory bindings;

    storage::configurable_provider storages;

    std::shared_ptr<storage::relation> t0 = storages.add_relation("T0", storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::relation> t1 = storages.add_relation("T1", storage::table {
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

    std::shared_ptr<storage::index> i0 = storages.add_index("I0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "I0",
            {
                    t0->columns()[0],
            },
            {
                    t0->columns()[1],
                    t0->columns()[2],
            }
    });
    std::shared_ptr<storage::index> i1 = storages.add_index("I1", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t1),
            "I1",
            {
                    t1->columns()[0],
            },
            {
                    t1->columns()[1],
                    t1->columns()[2],
            }
    });

    aggregate::configurable_provider aggregates;
    std::shared_ptr<aggregate::declaration> agg0 = aggregates.add(aggregate::declaration {
            aggregate::declaration::minimum_builtin_function_id + 1,
            "agg0",
            relation::set_quantifier::all,
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });
    std::shared_ptr<aggregate::declaration> agg1 = aggregates.add(aggregate::declaration {
            agg0->definition_id() + 1,
            "agg1",
            relation::set_quantifier::distinct,
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });
};

TEST_F(step_plan_builder_test, simple) {
    /*
     * scan:r0 - emit:ro
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
                    relation::find::key { t0c0, constant(100) },
            },
    });
    auto&& ro = r.insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    step_plan_builder builder;
    auto p = builder.build(std::move(r));

    ASSERT_EQ(p.size(), 1);
    auto&& p0 = find(p, r0);
    EXPECT_EQ(p0.operators().size(), 2);
    EXPECT_TRUE(p0.operators().contains(r0));
    EXPECT_TRUE(p0.operators().contains(ro));

    // find
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1m = r0.columns()[0].destination();

    ASSERT_EQ(r0.keys().size(), 1);
    EXPECT_EQ(r0.keys()[0].variable(), t0c0);
    EXPECT_EQ(r0.keys()[0].value(), constant(100));

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c1m);
}

TEST_F(step_plan_builder_test, escape) {
    /*
     * scan:r0 - escape:r1 - emit:ro
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
    auto&& ro = r.insert(relation::emit {
            x1,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    step_plan_builder builder;
    auto p = builder.build(std::move(r));

    ASSERT_EQ(p.size(), 1);
    auto&& p0 = find(p, r0);
    EXPECT_EQ(p0.operators().size(), 2);
    EXPECT_TRUE(p0.operators().contains(r0));
    EXPECT_TRUE(p0.operators().contains(ro));
    EXPECT_GT(r0.output(), ro.input());

    // scan
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1m = r0.columns()[0].destination();

    EXPECT_FALSE(r0.lower());
    EXPECT_FALSE(r0.upper());

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c1m);
}

TEST_F(step_plan_builder_test, join) {
    /*
     * scan:r0 -\
     *           join_relation:r2 - emit:r3
     * scan:r1 -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            scalar::compare {
                    scalar::comparison_operator::less,
                    scalar::variable_reference { cl1 },
                    constant(1),
            }
    });
    r2.key_pairs().emplace_back(cl0, cr0);
    auto& r3 = r.insert(relation::emit {
            cr2,
    });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();

    step_plan_builder builder;
    auto p = builder.build(std::move(r));

    ASSERT_EQ(p.size(), 5);

    auto&& p0 = find(p, r0);
    EXPECT_EQ(p0.operators().size(), 2);
    auto&& r4 = next<offer>(r0.output());

    auto&& e0 = resolve<plan::group>(r4.destination());

    auto&& p1 = find(p, r1);
    EXPECT_EQ(p1.operators().size(), 2);
    auto&& r5 = next<offer>(r1.output());

    auto&& e1 = resolve<plan::group>(r5.destination());

    auto&& p2 = find(p, r3);
    EXPECT_EQ(p2.operators().size(), 3);
    auto&& r7 = next<relation::step::join>(r3.input());
    auto&& r6 = next<take_cogroup>(r7.input());

    // el
    ASSERT_EQ(e0.columns().size(), 2);
    auto&& cl0el = e0.columns()[0];
    auto&& cl1el = e0.columns()[1];

    // er
    ASSERT_EQ(e1.columns().size(), 2);
    auto&& cr0er = e1.columns()[0];
    auto&& cr2er = e1.columns()[1];

    // scan - pl
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    auto&& cl0pl = r0.columns()[0].destination();
    auto&& cl1pl = r0.columns()[1].destination();

    // offer - pl
    ASSERT_EQ(r4.columns().size(), 2);
    EXPECT_EQ(r4.columns()[0].source(), cl0pl);
    EXPECT_EQ(r4.columns()[1].source(), cl1pl);
    EXPECT_EQ(r4.columns()[0].destination(), cl0el);
    EXPECT_EQ(r4.columns()[1].destination(), cl1el);

    // scan - pr
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), t1c0);
    EXPECT_EQ(r1.columns()[1].source(), t1c2);
    auto&& cr0pr = r1.columns()[0].destination();
    auto&& cr2pr = r1.columns()[1].destination();

    // offer - pr
    ASSERT_EQ(r5.columns().size(), 2);
    EXPECT_EQ(r5.columns()[0].source(), cr0pr);
    EXPECT_EQ(r5.columns()[1].source(), cr2pr);
    EXPECT_EQ(r5.columns()[0].destination(), cr0er);
    EXPECT_EQ(r5.columns()[1].destination(), cr2er);

    // take_cogroup
    ASSERT_EQ(r6.groups().size(), 2);
    auto&& gl = r6.groups()[0];
    auto&& gr = r6.groups()[1];

    ASSERT_EQ(gl.columns().size(), 1);
    EXPECT_EQ(gl.columns()[0].source(), cl1el);
    auto&& cl1pj = gl.columns()[0].destination();

    ASSERT_EQ(gr.columns().size(), 1);
    EXPECT_EQ(gr.columns()[0].source(), cr2er);
    auto&& cr2pj = gr.columns()[0].destination();

    // join_group
    EXPECT_EQ(r7.condition(), (scalar::compare {
            scalar::comparison_operator::less,
            scalar::variable_reference { cl1pj },
            constant(1),
    }));

    ASSERT_EQ(r3.columns().size(), 1);
    EXPECT_EQ(r3.columns()[0].source(), cr2pj);
}

} // namespace yugawara::analyzer
