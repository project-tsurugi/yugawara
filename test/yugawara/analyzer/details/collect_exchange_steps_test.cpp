#include <yugawara/analyzer/details/collect_exchange_steps.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
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
    using semantics = ::takatori::relation::comparison_semantics_kind;
    using scan_direction = ::takatori::relation::scan_direction;
    using sort_direction = ::takatori::relation::sort_direction;
    using emit_ordering = ::takatori::relation::emit_ordering;
    using forward_ordering = ::takatori::plan::forward_ordering;

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

    // CREATE INDEX I2 ON T0 (C0)
    std::shared_ptr<storage::index> i2 = storages.add_index({
            t0,
            "I2",
            {
                    t0->columns()[0],
            },
            {},
            {
                    storage::index_feature::find,
                    storage::index_feature::scan,
            },
    });

    struct ordered_limit_graph {
        relation::scan& scan;
        relation::intermediate::limit& limit;
        relation::emit& emit;
    };

    /*
     * scan:r0{I2, direction=D} - limit_relation:r1{sort=K, count=10} - emit:r2{ordering=O}
     */
    ordered_limit_graph build_ordered_limit(
            relation::graph_type& r,
            descriptor::variable const& c0,
            descriptor::variable const& c1,
            std::optional<scan_direction> direction,
            descriptor::variable const& sort_key,
            relation::emit::ordering_type ordering = {}) {
        auto& r0 = r.insert(relation::scan {
                bindings(*i2),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                },
                {},
                {},
                direction,
        });
        auto& r1 = r.insert(relation::intermediate::limit {
                {},
                {
                        { sort_key, sort_direction::ascendant },
                },
                relation::intermediate::limit::row_slice_type { {}, 10 },
        });
        auto& r2 = r.insert(relation::emit {
                { c0 },
                ordering,
        });
        r0.output() >> r1.input();
        r1.output() >> r2.input();
        return { r0, r1, r2 };
    }

    struct chained_limit_graph {
        relation::scan& scan;
        relation::intermediate::limit& upstream;
        relation::intermediate::limit& downstream;
        relation::emit& emit;
    };

    /*
     * scan:r0{I2, direction=forward} - limit_relation:r1{sort=c0, count=10} - [project:rp]
     *   - limit_relation:r2{sort=K, count=5} - emit:r3{ordering=O}
     */
    chained_limit_graph build_chained_limits(
            relation::graph_type& r,
            descriptor::variable const& c0,
            descriptor::variable const& c1,
            descriptor::variable const& sort_key,
            relation::emit::ordering_type ordering = {},
            bool with_project = false) {
        auto& r0 = r.insert(relation::scan {
                bindings(*i2),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                },
                {},
                {},
                scan_direction::forward,
        });
        auto& r1 = r.insert(relation::intermediate::limit {
                {},
                {
                        { c0, sort_direction::ascendant },
                },
                relation::intermediate::limit::row_slice_type { {}, 10 },
        });
        auto& r2 = r.insert(relation::intermediate::limit {
                {},
                {
                        { sort_key, sort_direction::ascendant },
                },
                relation::intermediate::limit::row_slice_type { {}, 5 },
        });
        auto& r3 = r.insert(relation::emit {
                { c0 },
                ordering,
        });
        r0.output() >> r1.input();
        if (with_project) {
            auto& rp = r.insert(relation::project {
                    relation::project::column { constant(), bindings.stream_variable("x") },
            });
            r1.output() >> rp.input();
            rp.output() >> r2.input();
        } else {
            r1.output() >> r2.input();
        }
        r2.output() >> r3.input();
        return { r0, r1, r2, r3 };
    }

    /*
     * scan:r0{I2, direction=D, count=N}
     */
    relation::scan& add_scan(
            relation::graph_type& r,
            descriptor::variable const& c0,
            descriptor::variable const& c1,
            std::optional<scan_direction> direction = {},
            std::optional<std::size_t> count = {}) {
        auto& result = r.insert(relation::scan {
                bindings(*i2),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                },
                {},
                {},
                direction,
        });
        if (count) {
            result.row_slice() = relation::scan::row_slice_type { {}, *count };
        }
        return result;
    }

    static relation::intermediate::limit limit(
            std::optional<std::size_t> count,
            std::vector<relation::intermediate::limit::sort_key> sort_keys = {}) {
        relation::intermediate::limit::row_slice_type slice {};
        if (count) {
            slice = relation::intermediate::limit::row_slice_type { {}, *count };
        }
        return relation::intermediate::limit {
                std::vector<descriptor::variable> {},
                std::move(sort_keys),
                std::move(slice),
        };
    }
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
    EXPECT_FALSE(e0.row_slice());

    ASSERT_EQ(e1.group_keys().size(), 0);
    EXPECT_FALSE(e1.row_slice());

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
                            semantics::ternary,
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                            semantics::ternary,
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
    EXPECT_FALSE(e0.row_slice());

    ASSERT_EQ(e1.group_keys().size(), 1);
    ASSERT_EQ(e1.group_keys()[0], c2);
    EXPECT_FALSE(e1.row_slice());

    EXPECT_EQ(r7.condition(), compare(c0, c2));
}

TEST_F(collect_exchange_steps_test, join_cogroup_mandatory) {
    // the co-group input can be mandatory only if the join outputs nothing for the keys where the input is empty
    struct expectation {
        relation::join_kind kind;
        bool left_mandatory;
        bool right_mandatory;
    };
    std::vector<expectation> const expectations {
            { relation::join_kind::inner, true, true },
            { relation::join_kind::left_outer, true, false },
            { relation::join_kind::left_outer_at_most_one, true, false },
            { relation::join_kind::full_outer, false, false },
            { relation::join_kind::semi, true, true },
            { relation::join_kind::anti, true, false },
    };
    for (auto&& e : expectations) {
        SCOPED_TRACE(e.kind);
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
                e.kind,
                compare(c0, c2),
        });
        auto& r3 = r.insert(relation::emit {
                c1,
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

        auto&& r7 = next<relation::step::join>(r3.input());
        auto&& r6 = next<take_cogroup>(r7.input());
        EXPECT_EQ(r7.operator_kind(), e.kind);

        ASSERT_EQ(r6.groups().size(), 2);
        EXPECT_EQ(r6.groups()[0].mandatory(), e.left_mandatory);
        EXPECT_EQ(r6.groups()[1].mandatory(), e.right_mandatory);
    }
}

TEST_F(collect_exchange_steps_test, join_cogroup_is_not_distinct_from) {
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
                            semantics::binary,
                    },
            },
            relation::endpoint_kind::prefixed_inclusive,
    };
    r2.upper() = relation::intermediate::join::endpoint {
            {
                    relation::intermediate::join::key {
                            c2,
                            varref { c0 },
                            semantics::binary,
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
    EXPECT_FALSE(e0.row_slice());

    ASSERT_EQ(e1.group_keys().size(), 1);
    ASSERT_EQ(e1.group_keys()[0], c2);
    EXPECT_FALSE(e1.row_slice());

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
    EXPECT_FALSE(e0.row_slice());

    ASSERT_EQ(e1.group_keys().size(), 1);
    ASSERT_EQ(e1.group_keys()[0], c2);
    EXPECT_FALSE(e1.row_slice());

    EXPECT_EQ(r7.condition(), compare(c0, c2));
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
    EXPECT_FALSE(e0.row_slice());

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
    EXPECT_FALSE(e0.row_slice());

    ASSERT_EQ(r5.columns().size(), 1);
    EXPECT_EQ(r5.columns()[0].function(), bindings(func));
    ASSERT_EQ(r5.columns()[0].arguments().size(), 1);
    EXPECT_EQ(r5.columns()[0].arguments()[0], c1);
    EXPECT_EQ(r5.columns()[0].destination(), c2);
}

TEST_F(collect_exchange_steps_test, aggregate_group_duplicate_keys) {
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
    EXPECT_FALSE(e0.row_slice());

    ASSERT_EQ(r5.columns().size(), 1);
    EXPECT_EQ(r5.columns()[0].function(), bindings(func));
    ASSERT_EQ(r5.columns()[0].arguments().size(), 1);
    EXPECT_EQ(r5.columns()[0].arguments()[0], c1);
    EXPECT_EQ(r5.columns()[0].destination(), c2);
}

TEST_F(collect_exchange_steps_test, aggregate_exchange_duplicate_keys) {
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
    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 1 }));
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
            relation::intermediate::limit::row_slice_type { {}, 10 },
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

    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
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
            {
                    c0,
                    c1,
            },
            {},
            relation::intermediate::limit::row_slice_type { {}, 10 },
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
    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=10} - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_ordered_limit(r, c0, c1, scan_direction::forward, c0);

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [forward{limit=10, ordering=respect_input}] - take_flat:r4 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 4);
    EXPECT_TRUE(r.contains(g.scan));
    EXPECT_TRUE(r.contains(g.emit));

    auto&& r3 = next<offer>(g.scan.output());
    auto&& r4 = next<relation::step::take_flat>(g.emit.input());

    auto&& e0 = resolve<plan::forward>(r3.destination());

    ASSERT_EQ(p.size(), 1);
    EXPECT_TRUE(p.contains(e0));

    EXPECT_EQ(r4.source(), r3.destination());

    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
    EXPECT_EQ(e0.ordering(), plan::forward::ordering_type { forward_ordering::respect_input });
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_preserve_output) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=10} - emit:r2{ordering=preserve_output}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_ordered_limit(
            r, c0, c1, scan_direction::forward, c0,
            { emit_ordering::preserve_output });

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [forward{limit=10, ordering=respect_input,preserve_output}] - take_flat:r4 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 4);

    auto&& r3 = next<offer>(g.scan.output());
    auto&& r4 = next<relation::step::take_flat>(g.emit.input());
    EXPECT_EQ(r4.source(), r3.destination());

    auto&& e0 = resolve<plan::forward>(r3.destination());
    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
    EXPECT_EQ(e0.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
            forward_ordering::preserve_output,
    }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_through_order_preserving) {
    /*
     * scan:r0{direction=forward} - filter:r1 - limit_relation:r2{sort=c0, count=10} - project:r3
     *   - emit:r4{ordering=preserve_output}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = r.insert(relation::scan {
            bindings(*i2),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
            {},
            {},
            scan_direction::forward,
    });
    auto& r1 = r.insert(relation::filter {
            compare(varref { c1 }, constant(1)),
    });
    auto& r2 = r.insert(relation::intermediate::limit {
            {},
            {
                    c0,
            },
            relation::intermediate::limit::row_slice_type { {}, 10 },
    });
    auto& r3 = r.insert(relation::project {
            relation::project::column { constant(), c2 },
    });
    auto& r4 = r.insert(relation::emit {
            { c0, c2 },
            { emit_ordering::preserve_output },
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - filter:r1 - offer:r5 - [forward{limit=10, ordering=respect_input,preserve_output}] - take_flat:r6
     *   - project:r3 - emit:r4
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(r.size(), 6);

    auto&& r5 = next<offer>(r1.output());
    auto&& r6 = next<relation::step::take_flat>(r3.input());
    EXPECT_EQ(r6.source(), r5.destination());

    auto&& e0 = resolve<plan::forward>(r5.destination());
    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
    EXPECT_EQ(e0.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
            forward_ordering::preserve_output,
    }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_without_direction) {
    /*
     * scan:r0{direction=()} - limit_relation:r1{sort=c0, count=10} - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_ordered_limit(r, c0, c1, {}, c0);

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [group{sort=c0, limit=10}] - take_group:r4 - flatten:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    auto&& r3 = next<offer>(g.scan.output());
    auto&& r5 = next<flatten>(g.emit.input());
    auto&& r4 = next<take_group>(r5.input());
    EXPECT_EQ(r4.source(), r3.destination());

    auto&& e0 = resolve<plan::group>(r3.destination());
    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_unsatisfied) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c1, count=10} - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_ordered_limit(r, c0, c1, scan_direction::forward, c1);

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [group{sort=c1, limit=10}] - take_group:r4 - flatten:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    auto&& r3 = next<offer>(g.scan.output());
    auto&& r5 = next<flatten>(g.emit.input());
    auto&& r4 = next<take_group>(r5.input());
    EXPECT_EQ(r4.source(), r3.destination());

    auto&& e0 = resolve<plan::group>(r3.destination());
    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_disabled) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=10} - emit:r2{ordering=preserve_output}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_ordered_limit(
            r, c0, c1, scan_direction::forward, c0,
            { emit_ordering::preserve_output });

    details::step_plan_builder_options options;
    options.runtime_features().erase(runtime_feature::ordered_forward);
    plan::graph_type p;

    /*
     * scan:r0 - offer:r3 - [group{sort=c0, limit=10}] - take_group:r4 - flatten:r5 - emit:r2
     */
    details::collect_exchange_steps(r, p, options);
    auto&& r3 = next<offer>(g.scan.output());
    auto&& r5 = next<flatten>(g.emit.input());
    auto&& r4 = next<take_group>(r5.input());
    EXPECT_EQ(r4.source(), r3.destination());

    auto&& e0 = resolve<plan::group>(r3.destination());
    ASSERT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_merge) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=10} - limit_relation:r2{sort=c0, count=5}
     *   - emit:r3{ordering=preserve_output}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_chained_limits(r, c0, c1, c0, { emit_ordering::preserve_output });

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer - [forward{limit=5, ordering=respect_input,preserve_output}] - take_flat - emit:r3
     * the row count of the downstream limit is merged into the upstream, and then the downstream is removed
     */
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(g.scan.output());
    auto&& t0 = next<relation::step::take_flat>(g.emit.input());
    ASSERT_EQ(p.size(), 1);
    EXPECT_EQ(o0.destination(), t0.source());

    auto&& e0 = resolve<plan::forward>(o0.destination());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 5 }));
    EXPECT_EQ(e0.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
            forward_ordering::preserve_output,
    }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_chain_through_order_preserving) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=10} - project
     *   - limit_relation:r2{sort=c0, count=5} - emit:r3{ordering=preserve_output}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_chained_limits(r, c0, c1, c0, { emit_ordering::preserve_output }, true);

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer - [forward{limit=10, ordering=respect_input,preserve_output}] - take_flat - project
     *   - offer - [forward{limit=5, ordering=respect_input,preserve_output}] - take_flat - emit:r3
     */
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(g.scan.output());
    auto&& t1 = next<relation::step::take_flat>(g.emit.input());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    auto&& e1 = resolve<plan::forward>(t1.source());
    ASSERT_EQ(p.size(), 2);

    EXPECT_EQ(e0.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
            forward_ordering::preserve_output,
    }));
    EXPECT_EQ(e1.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
            forward_ordering::preserve_output,
    }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_chain_without_ordered_emit) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=10} - project
     *   - limit_relation:r2{sort=c0, count=5} - emit:r3
     * (the project prevents from merging the limits, see limit_ordered_forward_merge)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_chained_limits(r, c0, c1, c0, {}, true);

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer - [forward{limit=10, ordering=respect_input,preserve_output}] - take_flat
     *   - offer - [forward{limit=5, ordering=respect_input}] - take_flat - emit:r3
     * the upstream preserves its output order, because the downstream depends on it
     */
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(g.scan.output());
    auto&& t1 = next<relation::step::take_flat>(g.emit.input());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    auto&& e1 = resolve<plan::forward>(t1.source());
    ASSERT_EQ(p.size(), 2);

    EXPECT_EQ(e0.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
            forward_ordering::preserve_output,
    }));
    EXPECT_EQ(e1.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
    }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_chain_unsatisfied) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=10} - limit_relation:r2{sort=c1, count=5}
     *   - emit:r3{ordering=preserve_output}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto g = build_chained_limits(r, c0, c1, c1, { emit_ordering::preserve_output });

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer - [forward{limit=10, ordering=respect_input}] - take_flat
     *   - offer - [group{sort=c1, limit=5}] - take_group - flatten - emit:r3
     * the downstream does not depend on the order of the upstream
     */
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(g.scan.output());
    auto&& f1 = next<flatten>(g.emit.input());
    auto&& t1 = next<take_group>(f1.input());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    auto&& e1 = resolve<plan::group>(t1.source());
    ASSERT_EQ(p.size(), 2);

    EXPECT_EQ(e0.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
    }));
    EXPECT_EQ(e1.row_slice(), (relation::details::row_slice { {}, 5 }));
}

TEST_F(collect_exchange_steps_test, limit_ordered_forward_not_through_exchange) {
    /*
     * scan:r0{direction=forward} - distinct_relation:r1{c0, c1} - limit_relation:r2{sort=c0, count=10}
     *   - emit:r3{ordering=preserve_output}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i2),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
            {},
            {},
            scan_direction::forward,
    });
    auto& r1 = r.insert(relation::intermediate::distinct {
            c0,
            c1,
    });
    auto& r2 = r.insert(relation::intermediate::limit {
            {},
            {
                    c0,
            },
            relation::intermediate::limit::row_slice_type { {}, 10 },
    });
    auto& r3 = r.insert(relation::emit {
            { c0 },
            { emit_ordering::preserve_output },
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;

    /*
     * scan:r0 - offer - [group{c0, c1}] - take_group - flatten
     *   - offer - [group{sort=c0, limit=10}] - take_group:r5 - flatten:r4 - emit:r3
     */
    details::collect_exchange_steps(r, p, options);
    ASSERT_EQ(p.size(), 2);

    auto&& r4 = next<flatten>(r3.input());
    auto&& r5 = next<take_group>(r4.input());
    auto&& e1 = resolve<plan::group>(r5.source());
    EXPECT_EQ(e1.row_slice(), (relation::details::row_slice { {}, 10 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_count) {
    /*
     * scan:r0{count=3} - limit_relation:r1{count=3} - emit:r2
     * => scan:r0{count=3} - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, {}, 3);
    auto& r1 = r.insert(limit(3));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
    EXPECT_EQ(p.size(), 0);
}

TEST_F(collect_exchange_steps_test, limit_redundant_count_less) {
    /*
     * scan:r0{count=3} - limit_relation:r1{count=5} - emit:r2
     * => scan:r0{count=3} - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, {}, 3);
    auto& r1 = r.insert(limit(5));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
    EXPECT_EQ(p.size(), 0);
}

TEST_F(collect_exchange_steps_test, limit_redundant_count_greater) {
    /*
     * scan:r0{count=5} - limit_relation:r1{count=3} - emit:r2
     * => scan:r0{count=5} - offer - [forward{limit=3}] - take_flat - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, {}, 5);
    auto& r1 = r.insert(limit(3));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    ASSERT_EQ(p.size(), 1);
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_count_without_scan_count) {
    /*
     * scan:r0 - limit_relation:r1{count=3} - emit:r2
     * => scan:r0 - offer - [forward{limit=3}] - take_flat - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(3));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    ASSERT_EQ(p.size(), 1);
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_count_disabled) {
    /*
     * scan:r0{count=3} - limit_relation:r1{count=3} - emit:r2
     * => scan:r0{count=3} - offer - [forward{limit=3}] - take_flat - emit:r2 (if scan_limit is disabled)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, {}, 3);
    auto& r1 = r.insert(limit(3));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    options.runtime_features().erase(runtime_feature::scan_limit);
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    ASSERT_EQ(p.size(), 1);
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_ordered) {
    /*
     * scan:r0{direction=forward, count=3} - limit_relation:r1{sort=c0, count=3} - emit:r2
     * => scan:r0{direction=forward, count=3} - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward, 3);
    auto& r1 = r.insert(limit(3, { c0 }));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
    EXPECT_EQ(p.size(), 0);
}

TEST_F(collect_exchange_steps_test, limit_redundant_ordered_without_count) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0} - emit:r2
     * => scan:r0{direction=forward} - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward);
    auto& r1 = r.insert(limit({}, { c0 }));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
    EXPECT_EQ(p.size(), 0);
}

TEST_F(collect_exchange_steps_test, limit_redundant_ordered_without_direction) {
    /*
     * scan:r0{count=3} - limit_relation:r1{sort=c0, count=3} - emit:r2
     * => scan:r0{count=3} - offer - [group{sort=c0, limit=3}] - take_group - flatten - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, {}, 3);
    auto& r1 = r.insert(limit(3, { c0 }));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::group>(o0.destination());
    ASSERT_EQ(p.size(), 1);
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_ordered_unsatisfied) {
    /*
     * scan:r0{direction=forward, count=3} - limit_relation:r1{sort=c1, count=3} - emit:r2
     * => scan:r0{direction=forward, count=3} - offer - [group{sort=c1, limit=3}] - take_group - flatten - emit:r2
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward, 3);
    auto& r1 = r.insert(limit(3, { c1 }));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::group>(o0.destination());
    ASSERT_EQ(p.size(), 1);
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_ordered_disabled) {
    /*
     * scan:r0{direction=forward, count=3} - limit_relation:r1{sort=c0, count=3} - emit:r2
     * => (not removed, if ordered_scan is disabled)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward, 3);
    auto& r1 = r.insert(limit(3, { c0 }));
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    options.runtime_features().erase(runtime_feature::ordered_scan);
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    ASSERT_EQ(p.size(), 1);
    auto&& e0 = resolve<plan::forward>(o0.destination());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_through_filter_project) {
    /*
     * scan:r0{direction=forward, count=3} - filter:r1 - project:r2 - limit_relation:r3{sort=c0, count=3} - emit:r4
     * => scan:r0{direction=forward, count=3} - filter:r1 - project:r2 - emit:r4
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto x = bindings.stream_variable("x");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward, 3);
    auto& r1 = r.insert(relation::filter {
            compare(varref { c1 }, constant(1)),
    });
    auto& r2 = r.insert(relation::project {
            relation::project::column { constant(), x },
    });
    auto& r3 = r.insert(limit(3, { c0 }));
    auto& r4 = r.insert(relation::emit { c0, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r2.output(), r4.input());
    EXPECT_EQ(p.size(), 0);
}

TEST_F(collect_exchange_steps_test, limit_redundant_start) {
    /*
     * scan:r0{count=3} - limit_relation:r1{start=2, count=3} - emit:r2
     * => (not removed)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, {}, 3);
    auto& r1 = r.insert(relation::intermediate::limit {
            relation::intermediate::limit::row_slice_type { 2, 3 },
    });
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    ASSERT_EQ(p.size(), 1);
    auto&& e0 = resolve<plan::forward>(o0.destination());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { 2, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_group_keys) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{group=c0, sort=c1} - emit:r2
     * => (not removed)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward);
    auto& r1 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> { c0 },
            std::vector<relation::intermediate::limit::sort_key> { c1 },
    });
    auto& r2 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    auto&& o0 = next<offer>(r0.output());
    ASSERT_EQ(p.size(), 1);
    auto&& e0 = resolve<plan::group>(o0.destination());
    ASSERT_EQ(e0.group_keys().size(), 1);
}

TEST_F(collect_exchange_steps_test, limit_redundant_empty) {
    /*
     * scan:r0 - distinct_relation:r1 - limit_relation:r2 - emit:r3
     * => scan:r0 - offer - [group{c0}] - take_group - flatten:r4 - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(relation::intermediate::distinct { c0 });
    auto& r2 = r.insert(limit({}));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r4 = next<flatten>(r3.input());
    auto&& t0 = next<take_group>(r4.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    ASSERT_EQ(e0.group_keys().size(), 1);
}

TEST_F(collect_exchange_steps_test, limit_merge_strengthen) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=10} - limit_relation:r2{sort=(c0, c1), count=3} - emit:r3
     * => scan:r0 - offer - [group{sort=(c0, c1), limit=3}] - take_group - flatten:r4 - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(10, { c0 }));
    auto& r2 = r.insert(limit(3, { c0, c1 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r4 = next<flatten>(r3.input());
    auto&& t0 = next<take_group>(r4.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
    ASSERT_EQ(e0.sort_keys().size(), 2);
    EXPECT_EQ(e0.sort_keys()[0].variable(), c0);
    EXPECT_EQ(e0.sort_keys()[1].variable(), c1);
}

TEST_F(collect_exchange_steps_test, limit_merge_strengthen_without_upstream_count) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0} - limit_relation:r2{sort=(c0, c1), count=3} - emit:r3
     * => scan:r0 - offer - [group{sort=(c0, c1), limit=3}] - take_group - flatten:r4 - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit({}, { c0 }));
    auto& r2 = r.insert(limit(3, { c0, c1 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r4 = next<flatten>(r3.input());
    auto&& t0 = next<take_group>(r4.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
    EXPECT_EQ(e0.sort_keys().size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_merge_shrink) {
    /*
     * scan:r0 - limit_relation:r1{sort=(c0, c1), count=10} - limit_relation:r2{sort=c0, count=3} - emit:r3
     * => scan:r0 - offer - [group{sort=(c0, c1), limit=3}] - take_group - flatten:r4 - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(10, { c0, c1 }));
    auto& r2 = r.insert(limit(3, { c0 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r4 = next<flatten>(r3.input());
    auto&& t0 = next<take_group>(r4.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
    EXPECT_EQ(e0.sort_keys().size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_merge_chain) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=10} - limit_relation:r2{sort=c0, count=5}
     *         - limit_relation:r3{sort=(c0, c1), count=3} - emit:r4
     * => scan:r0 - offer - [group{sort=(c0, c1), limit=3}] - take_group - flatten:r5 - emit:r4
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(10, { c0 }));
    auto& r2 = r.insert(limit(5, { c0 }));
    auto& r3 = r.insert(limit(3, { c0, c1 }));
    auto& r4 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r5 = next<flatten>(r4.input());
    auto&& t0 = next<take_group>(r5.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
    EXPECT_EQ(e0.sort_keys().size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_merge_strengthen_ordered_upstream) {
    /*
     * scan:r0{forward} - limit_relation:r1{sort=c0, count=10} - limit_relation:r2{sort=(c0, c1), count=3} - emit:r3
     * => (not merged: the upstream limit becomes a forward exchange with ordering)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward);
    auto& r1 = r.insert(limit(10, { c0 }));
    auto& r2 = r.insert(limit(3, { c0, c1 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    EXPECT_EQ(p.size(), 2);
    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
}

TEST_F(collect_exchange_steps_test, limit_merge_strengthen_ordered_upstream_through_filter) {
    /*
     * scan:r0{forward} - filter:r1 - limit_relation:r2{sort=c0, count=3} - limit_relation:r3{sort=(c0, c1), count=3}
     *   - emit:r4
     * => scan:r0 - filter:r1 - offer - [forward{limit=3, ordering=respect_input}] - take_flat
     *   - offer - [group{sort=(c0, c1), limit=3}] - take_group - flatten - emit:r4
     * (not merged: the upstream limit becomes a forward exchange with ordering, which stops early)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward);
    auto& r1 = r.insert(relation::filter {
            compare(varref { c1 }, constant(1)),
    });
    auto& r2 = r.insert(limit(3, { c0 }));
    auto& r3 = r.insert(limit(3, { c0, c1 }));
    auto& r4 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 2);
    auto&& o0 = next<offer>(r1.output());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
    EXPECT_TRUE(e0.ordering().contains(forward_ordering::respect_input));

    auto&& f1 = next<flatten>(r4.input());
    auto&& t1 = next<take_group>(f1.input());
    auto&& e1 = resolve<plan::group>(t1.source());
    EXPECT_EQ(e1.row_slice(), (relation::details::row_slice { {}, 3 }));
    EXPECT_EQ(e1.sort_keys().size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_merge_strengthen_without_upstream_sort_keys) {
    /*
     * scan:r0 - limit_relation:r1{count=10} - limit_relation:r2{sort=c0, count=3} - emit:r3
     * => (not merged: the upstream limit becomes a flat forward exchange)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(10));
    auto& r2 = r.insert(limit(3, { c0 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    EXPECT_EQ(p.size(), 2);
    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 10 }));
}

TEST_F(collect_exchange_steps_test, limit_merge_not_prefix) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=10} - limit_relation:r2{sort=c1, count=3} - emit:r3
     * => (not merged)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(10, { c0 }));
    auto& r2 = r.insert(limit(3, { c1 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    EXPECT_EQ(p.size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_merge_variable_count) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=n} - limit_relation:r2{sort=(c0, c1), count=3} - emit:r3
     * => (not merged: the row counts cannot be merged)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto n = bindings.stream_variable("n");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { {}, n },
    });
    auto& r2 = r.insert(limit(3, { c0, c1 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    EXPECT_EQ(p.size(), 2);
    auto&& o0 = next<offer>(r0.output());
    auto&& e0 = resolve<plan::group>(o0.destination());
    EXPECT_EQ(e0.sort_keys().size(), 1);
}

TEST_F(collect_exchange_steps_test, limit_redundant_upstream_limit) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=3} - limit_relation:r2{sort=c0, count=5} - emit:r3
     * => scan:r0 - offer - [group{sort=c0, limit=3}] - take_group - flatten:r4 - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(3, { c0 }));
    auto& r2 = r.insert(limit(5, { c0 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r4 = next<flatten>(r3.input());
    auto&& t0 = next<take_group>(r4.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 3 }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_upstream_limit_count_greater) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=5} - project:rp - limit_relation:r2{sort=c0, count=3} - emit:r3
     * => (not removed)
     * (the project prevents from merging the limits, see limit_merge_*)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto x = bindings.stream_variable("x");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(5, { c0 }));
    auto& rp = r.insert(relation::project {
            relation::project::column { constant(), x },
    });
    auto& r2 = r.insert(limit(3, { c0 }));
    auto& r3 = r.insert(relation::emit { c0, x });
    r0.output() >> r1.input();
    r1.output() >> rp.input();
    rp.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    EXPECT_EQ(p.size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_redundant_upstream_limit_sort_keys_prefix) {
    /*
     * scan:r0 - limit_relation:r1{sort=(c0, c1), count=3} - limit_relation:r2{sort=c0, count=5} - emit:r3
     * => scan:r0 - offer - [group{sort=(c0, c1), limit=3}] - take_group - flatten:r4 - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(3, { c0, c1 }));
    auto& r2 = r.insert(limit(5, { c0 }));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r4 = next<flatten>(r3.input());
    auto&& t0 = next<take_group>(r4.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    EXPECT_EQ(e0.sort_keys().size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_redundant_upstream_limit_sort_keys_not_prefix) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=3} - project:rp - limit_relation:r2{sort=(c0, c1), count=5} - emit:r3
     * => (not removed)
     * (the project prevents from merging the limits, see limit_merge_*)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto x = bindings.stream_variable("x");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(limit(3, { c0 }));
    auto& rp = r.insert(relation::project {
            relation::project::column { constant(), x },
    });
    auto& r2 = r.insert(limit(5, { c0, c1 }));
    auto& r3 = r.insert(relation::emit { c0, x });
    r0.output() >> r1.input();
    r1.output() >> rp.input();
    rp.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    EXPECT_EQ(p.size(), 2);
}

TEST_F(collect_exchange_steps_test, limit_redundant_upstream_limit_same_variable_count) {
    /*
     * scan:r0 - limit_relation:r1{sort=c0, count=n} - limit_relation:r2{sort=c0, count=n} - emit:r3
     * => scan:r0 - offer - [group{sort=c0, limit=n}] - take_group - flatten:r4 - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto n = bindings.stream_variable("n");
    auto& r0 = add_scan(r, c0, c1);
    auto& r1 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { {}, n },
    });
    auto& r2 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { {}, n },
    });
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& r4 = next<flatten>(r3.input());
    auto&& t0 = next<take_group>(r4.input());
    auto&& e0 = resolve<plan::group>(t0.source());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, n }));
}

TEST_F(collect_exchange_steps_test, limit_redundant_chain) {
    /*
     * scan:r0{count=3} - limit_relation:r1{count=5} - limit_relation:r2{count=7} - emit:r3
     * => scan:r0{count=3} - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, {}, 3);
    auto& r1 = r.insert(limit(5));
    auto& r2 = r.insert(limit(7));
    auto& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r3.input());
    EXPECT_EQ(p.size(), 0);
}

TEST_F(collect_exchange_steps_test, limit_redundant_preserve_output_through_removed) {
    /*
     * scan:r0{direction=forward} - limit_relation:r1{sort=c0, count=5} - limit_relation:r2{sort=c0, count=10}
     *   - emit:r3{ordering=preserve_output}
     * => scan:r0 - offer - [forward{limit=5, ordering=respect_input,preserve_output}] - take_flat - emit:r3
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = add_scan(r, c0, c1, scan_direction::forward);
    auto& r1 = r.insert(limit(5, { c0 }));
    auto& r2 = r.insert(limit(10, { c0 }));
    auto& r3 = r.insert(relation::emit {
            { c0 },
            { emit_ordering::preserve_output },
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    details::step_plan_builder_options options;
    plan::graph_type p;
    details::collect_exchange_steps(r, p, options);

    ASSERT_EQ(p.size(), 1);
    auto&& o0 = next<offer>(r0.output());
    auto&& t0 = next<relation::step::take_flat>(r3.input());
    EXPECT_EQ(t0.source(), o0.destination());
    auto&& e0 = resolve<plan::forward>(o0.destination());
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 5 }));
    EXPECT_EQ(e0.ordering(), (plan::forward::ordering_type {
            forward_ordering::respect_input,
            forward_ordering::preserve_output,
    }));
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
    EXPECT_FALSE(e0.row_slice());

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
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 1 }));

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
    EXPECT_FALSE(e0.row_slice());

    ASSERT_EQ(e1.group_keys().size(), 2);
    EXPECT_EQ(e1.group_keys()[0], cr0);
    EXPECT_EQ(e1.group_keys()[1], cr1);
    EXPECT_FALSE(e1.row_slice());
}

TEST_F(collect_exchange_steps_test, difference_distinct) {
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
     *                                   take_cogroup:r6 - join_group[anti]:r7 - emit:r3
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

    EXPECT_EQ(r7.operator_kind(), relation::join_kind::anti);
    EXPECT_FALSE(r7.condition());

    ASSERT_EQ(e0.group_keys().size(), 2);
    EXPECT_EQ(e0.group_keys()[0], cl0);
    EXPECT_EQ(e0.group_keys()[1], cl1);
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 1 }));

    ASSERT_EQ(e1.group_keys().size(), 2);
    EXPECT_EQ(e1.group_keys()[0], cr0);
    EXPECT_EQ(e1.group_keys()[1], cr1);
    EXPECT_EQ(e1.row_slice(), (relation::details::row_slice { {}, 1 }));
}

TEST_F(collect_exchange_steps_test, intersection_distinct) {
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
     *                                   take_cogroup:r6 - join_group[semi]:r7 - emit:r3
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

    EXPECT_EQ(r7.operator_kind(), relation::join_kind::semi);
    EXPECT_FALSE(r7.condition());

    ASSERT_EQ(e0.group_keys().size(), 2);
    EXPECT_EQ(e0.group_keys()[0], cl0);
    EXPECT_EQ(e0.group_keys()[1], cl1);
    EXPECT_EQ(e0.row_slice(), (relation::details::row_slice { {}, 1 }));

    ASSERT_EQ(e1.group_keys().size(), 2);
    EXPECT_EQ(e1.group_keys()[0], cr0);
    EXPECT_EQ(e1.group_keys()[1], cr1);
    EXPECT_EQ(e1.row_slice(), (relation::details::row_slice { {}, 1 }));
}

} // namespace yugawara::analyzer::details
