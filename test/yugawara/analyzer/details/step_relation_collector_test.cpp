#include <yugawara/analyzer/details/collect_step_relations.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/limit.h>

#include <takatori/relation/step/join.h>

#include <takatori/plan/graph.h>
#include <takatori/plan/forward.h>
#include <takatori/plan/group.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include <yugawara/analyzer/details/collect_exchange_steps.h>
#include <yugawara/analyzer/details/collect_process_steps.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class step_relation_collector_test : public ::testing::Test {
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
    descriptor::variable t1c0 = bindings(t1->columns()[0]);

    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages.add_index({ t1, "I1" });

    aggregate::configurable_provider aggregates;
    std::shared_ptr<aggregate::declaration> agg0 = aggregates.add(aggregate::declaration {
            aggregate::declaration::minimum_builtin_function_id + 1,
            "testing",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });
};

template<class Seq, class T>
static bool contains(Seq const& seq, T const& t) {
    for (auto&& e : seq) {
        if (std::addressof(e) == std::addressof(t)) {
            return true;
        }
    }
    return false;
}


TEST_F(step_relation_collector_test, simple) {
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

    details::step_plan_builder_options info;
    plan::graph_type p;

    /*
     * [scan:r0 - emit:r1]:p0
     */
    details::collect_exchange_steps(r, p, info);
    details::collect_process_steps(std::move(r), p);
    details::collect_step_relations(p);

    ASSERT_EQ(p.size(), 1);
    auto&& p0 = find(p, r0);

    ASSERT_EQ(p0.upstreams().size(), 0);
    ASSERT_EQ(p0.downstreams().size(), 0);
}

TEST_F(step_relation_collector_test, straight) {
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

    details::step_plan_builder_options info;
    plan::graph_type p;

    /*
     * [scan:r0 - offer:r3]:p0 - [forward]:e0 - [take_flat:r4 - emit:r2]:p1
     */
    details::collect_exchange_steps(r, p, info);
    auto&& r3 = next<offer>(r0.output());

    details::collect_process_steps(std::move(r), p);
    details::collect_step_relations(p);

    ASSERT_EQ(p.size(), 3);
    auto&& p0 = find(p, r0);
    auto&& e0 = resolve<plan::forward>(r3.destination());
    auto&& p1 = find(p, r2);

    ASSERT_EQ(p0.upstreams().size(), 0);
    ASSERT_EQ(p0.downstreams().size(), 1);
    EXPECT_TRUE(contains(p0.downstreams(), e0));

    ASSERT_EQ(p1.upstreams().size(), 1);
    EXPECT_TRUE(contains(p1.upstreams(), e0));
    ASSERT_EQ(p1.downstreams().size(), 0);
}

TEST_F(step_relation_collector_test, gather) {
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

    details::step_plan_builder_options info;
    plan::graph_type p;

    /*
     * [scan:r0 - offer:r4]:p0 - [group]:e0 -\
     *                                        [take_cogroup:r6 - join_group:r7 - emit:r3]:p2
     * [scan:r1 - offer:r5]:p1 - [group]:e1 -/
     */
    details::collect_exchange_steps(r, p, info);
    ASSERT_EQ(r.size(), 7);
    EXPECT_TRUE(r.contains(r0));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_TRUE(r.contains(r3));

    auto&& r4 = next<offer>(r0.output());
    auto&& r5 = next<offer>(r1.output());
    auto&& r7 = next<relation::step::join>(r3.input());
    auto&& r6 = next<take_cogroup>(r7.input());

    details::collect_process_steps(std::move(r), p);
    details::collect_step_relations(p);

    ASSERT_EQ(p.size(), 5);
    auto&& p0 = find(p, r0);
    auto&& e0 = resolve<plan::group>(r4.destination());
    auto&& p1 = find(p, r1);
    auto&& e1 = resolve<plan::group>(r5.destination());
    auto&& p2 = find(p, r6);

    ASSERT_EQ(p0.upstreams().size(), 0);
    ASSERT_EQ(p0.downstreams().size(), 1);
    EXPECT_TRUE(contains(p0.downstreams(), e0));

    ASSERT_EQ(p1.upstreams().size(), 0);
    ASSERT_EQ(p1.downstreams().size(), 1);
    EXPECT_TRUE(contains(p1.downstreams(), e1));

    ASSERT_EQ(p2.upstreams().size(), 2);
    EXPECT_TRUE(contains(p2.upstreams(), e0));
    EXPECT_TRUE(contains(p2.upstreams(), e1));
    ASSERT_EQ(p2.downstreams().size(), 0);
}

} // namespace yugawara::analyzer::details
