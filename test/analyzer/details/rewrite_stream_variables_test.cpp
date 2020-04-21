#include <analyzer/details/rewrite_stream_variables.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>

#include <takatori/relation/intermediate/escape.h>

#include <takatori/relation/step/join.h>
#include <takatori/relation/step/aggregate.h>
#include <takatori/relation/step/intersection.h>
#include <takatori/relation/step/difference.h>
#include <takatori/relation/step/flatten.h>

#include <takatori/plan/graph.h>
#include <takatori/plan/forward.h>
#include <takatori/plan/group.h>
#include <takatori/plan/aggregate.h>
#include <takatori/plan/broadcast.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include <analyzer/details/collect_step_relations.h>
#include <analyzer/details/collect_exchange_columns.h>

#include "utils.h"

namespace yugawara::analyzer::details {

class rewrite_stream_variables_test : public ::testing::Test {
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
    });
    std::shared_ptr<storage::index> i1 = storages.add_index("I1", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t1),
            "I1",
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

    void apply(plan::graph_type& graph) {
        details::collect_step_relations(graph);
        auto map = details::collect_exchange_columns(graph, creator);
        details::rewrite_stream_variables(map, graph, creator);
    }
};

TEST_F(rewrite_stream_variables_test, find) {
    /*
     * [scan:r0 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::find {
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
    auto&& ro = p0.operators().insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    apply(p);
    auto&& c1m = ro.columns()[0].source();

    EXPECT_NE(c1m, c1);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[0].destination(), c1m);
}

TEST_F(rewrite_stream_variables_test, scan) {
    /*
     * [scan:r0 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& ro = p0.operators().insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    apply(p);
    auto&& c1m = ro.columns()[0].source();

    EXPECT_NE(c1m, c1);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[0].destination(), c1m);
}

TEST_F(rewrite_stream_variables_test, join_find_index) {
    /*
     * [scan:r0 - join_find:r1 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::join_find {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t1c0, j0 },
                    { t1c1, j1 },
                    { t1c2, j2 },
            },
            {
                    relation::join_find::key {
                            t1c0,
                            scalar::variable_reference { c0 },
                    },
            },
            compare(c1, j1),
    });
    auto&& ro = p0.operators().insert(relation::emit {
            j2,
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    auto&& c0m = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    auto&& c1m = r0.columns()[1].destination();

    // join_find
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), t1c1);
    auto&& j1m = r1.columns()[0].destination();
    EXPECT_EQ(r1.columns()[1].source(), t1c2);
    auto&& j2m = r1.columns()[1].destination();

    ASSERT_EQ(r1.keys().size(), 1);
    EXPECT_EQ(r1.keys()[0].variable(), t1c0);
    EXPECT_EQ(r1.keys()[0].value(), scalar::variable_reference(c0m));

    EXPECT_EQ(r1.condition(), compare(c1m, j1m));

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), j2m);
}

TEST_F(rewrite_stream_variables_test, join_find_broadcast) {
    /*
     *                                 [scan:r0 - join_find:r1 - emit:ro]:p0
     *                                            /
     * [scan:r2 - offer:r3]:p1 - [broadcast]:e0 -/
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    auto&& e0 = p.insert(plan::broadcast {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::join_find {
            relation::join_kind::inner,
            bindings(e0),
            {
            },
            {
                    relation::join_find::key {
                            j0,
                            scalar::variable_reference { c0 },
                    },
            },
            compare(c1, j1),
    });
    auto&& ro = p0.operators().insert(relation::emit {
            j2,
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    auto&& r2 = p1.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, j0 },
                    { t1c1, j1 },
                    { t1c2, j2 },
            },
    });
    auto&& r3 = p1.operators().insert(offer {
            bindings(e0)
    });
    r2.output() >> r3.input();

    apply(p);

    ASSERT_EQ(e0.columns().size(), 3);
    auto&& j0e0 = e0.columns()[0];
    auto&& j1e0 = e0.columns()[1];
    auto&& j2e0 = e0.columns()[2];

    // scan - p0
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    auto&& c0p0 = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    auto&& c1p0 = r0.columns()[1].destination();

    // join_find
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), j1e0);
    auto&& j1p0 = r1.columns()[0].destination();
    EXPECT_EQ(r1.columns()[1].source(), j2e0);
    auto&& j2p0 = r1.columns()[1].destination();

    ASSERT_EQ(r1.keys().size(), 1);
    EXPECT_EQ(r1.keys()[0].variable(), j0e0);
    EXPECT_EQ(r1.keys()[0].value(), scalar::variable_reference(c0p0));

    EXPECT_EQ(r1.condition(), compare(c1p0, j1p0));

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), j2p0);

    // scan - p1
    ASSERT_EQ(r2.columns().size(), 3);
    EXPECT_EQ(r2.columns()[0].source(), t1c0);
    auto&& j0p1 = r2.columns()[0].destination();
    EXPECT_EQ(r2.columns()[1].source(), t1c1);
    auto&& j1p1 = r2.columns()[1].destination();
    EXPECT_EQ(r2.columns()[2].source(), t1c2);
    auto&& j2p1 = r2.columns()[2].destination();

    // offer - p1
    ASSERT_EQ(r3.columns().size(), 3);
    EXPECT_EQ(r3.columns()[0].source(), j0p1);
    EXPECT_EQ(r3.columns()[0].destination(), j0e0);
    EXPECT_EQ(r3.columns()[1].source(), j1p1);
    EXPECT_EQ(r3.columns()[1].destination(), j1e0);
    EXPECT_EQ(r3.columns()[2].source(), j2p1);
    EXPECT_EQ(r3.columns()[2].destination(), j2e0);
}

TEST_F(rewrite_stream_variables_test, join_scan_index) {
    /*
     * [scan:r0 - join_scan:r1 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::join_scan {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t1c0, j0 },
                    { t1c1, j1 },
                    { t1c2, j2 },
            },
            {
                    relation::join_scan::key {
                            t1c0,
                            scalar::variable_reference { c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
            {},
            compare(c1, j1),
    });
    auto&& ro = p0.operators().insert(relation::emit {
            j2,
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    auto&& c0m = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    auto&& c1m = r0.columns()[1].destination();

    // join_scan
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), t1c1);
    auto&& j1m = r1.columns()[0].destination();
    EXPECT_EQ(r1.columns()[1].source(), t1c2);
    auto&& j2m = r1.columns()[1].destination();

    ASSERT_EQ(r1.lower().keys().size(), 1);
    EXPECT_EQ(r1.lower().keys()[0].variable(), t1c0);
    EXPECT_EQ(r1.lower().keys()[0].value(), scalar::variable_reference(c0m));

    EXPECT_EQ(r1.condition(), compare(c1m, j1m));

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), j2m);
}

TEST_F(rewrite_stream_variables_test, join_scan_broadcast) {
    /*
     *                                 [scan:r0 - join_scan:r1 - emit:ro]:p0
     *                                            /
     * [scan:r2 - offer:r3]:p1 - [broadcast]:e0 -/
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    auto&& e0 = p.insert(plan::broadcast {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::join_scan {
            relation::join_kind::inner,
            bindings(e0),
            {
            },
            {},
            {
                    relation::join_scan::key {
                            j0,
                            scalar::variable_reference { c0 },
                    },
                    relation::endpoint_kind::inclusive,
            },
            compare(c1, j1),
    });
    auto&& ro = p0.operators().insert(relation::emit {
            j2,
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    auto&& r2 = p1.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, j0 },
                    { t1c1, j1 },
                    { t1c2, j2 },
            },
    });
    auto&& r3 = p1.operators().insert(offer {
            bindings(e0)
    });
    r2.output() >> r3.input();

    apply(p);

    ASSERT_EQ(e0.columns().size(), 3);
    auto&& j0e0 = e0.columns()[0];
    auto&& j1e0 = e0.columns()[1];
    auto&& j2e0 = e0.columns()[2];

    // scan - p0
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    auto&& c0p0 = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    auto&& c1p0 = r0.columns()[1].destination();

    // join_scan
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), j1e0);
    auto&& j1p0 = r1.columns()[0].destination();
    EXPECT_EQ(r1.columns()[1].source(), j2e0);
    auto&& j2p0 = r1.columns()[1].destination();

    ASSERT_EQ(r1.upper().keys().size(), 1);
    EXPECT_EQ(r1.upper().keys()[0].variable(), j0e0);
    EXPECT_EQ(r1.upper().keys()[0].value(), scalar::variable_reference(c0p0));

    EXPECT_EQ(r1.condition(), compare(c1p0, j1p0));

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), j2p0);

    // scan - p1
    ASSERT_EQ(r2.columns().size(), 3);
    EXPECT_EQ(r2.columns()[0].source(), t1c0);
    auto&& j0p1 = r2.columns()[0].destination();
    EXPECT_EQ(r2.columns()[1].source(), t1c1);
    auto&& j1p1 = r2.columns()[1].destination();
    EXPECT_EQ(r2.columns()[2].source(), t1c2);
    auto&& j2p1 = r2.columns()[2].destination();

    // offer - p1
    ASSERT_EQ(r3.columns().size(), 3);
    EXPECT_EQ(r3.columns()[0].source(), j0p1);
    EXPECT_EQ(r3.columns()[0].destination(), j0e0);
    EXPECT_EQ(r3.columns()[1].source(), j1p1);
    EXPECT_EQ(r3.columns()[1].destination(), j1e0);
    EXPECT_EQ(r3.columns()[2].source(), j2p1);
    EXPECT_EQ(r3.columns()[2].destination(), j2e0);
}

TEST_F(rewrite_stream_variables_test, project) {
    /*
     * [scan:r0 - project:r1 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x0");
    auto x2 = bindings.stream_variable("x0");
    auto x3 = bindings.stream_variable("x3");
    auto&& r1 = p0.operators().insert(relation::project {
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
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c2 },
                            scalar::variable_reference { x1 },
                    },
                    x2,
            },
            relation::project::column {
                    constant(3),
                    x3,
            },
    });
    auto&& ro = p0.operators().insert(relation::emit {
            x2,
            x3,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    // scan - p0
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1p0 = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& c2p0 = r0.columns()[1].destination();

    // project
    ASSERT_EQ(r1.columns().size(), 3);
    auto&& x1p0 = r1.columns()[0].variable();
    auto&& x2p0 = r1.columns()[1].variable();
    auto&& x3p0 = r1.columns()[2].variable();
    EXPECT_EQ(r1.columns()[0].value(), (scalar::binary {
            scalar::binary_operator::add,
            scalar::variable_reference { c1p0 },
            constant(2),
    }));
    EXPECT_EQ(r1.columns()[1].value(), (scalar::binary {
            scalar::binary_operator::add,
            scalar::variable_reference { c2p0 },
            scalar::variable_reference { x1p0 },
    }));
    EXPECT_EQ(r1.columns()[2].value(), constant(3));

    // emit
    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), x2p0);
    EXPECT_EQ(ro.columns()[1].source(), x3p0);
}

TEST_F(rewrite_stream_variables_test, filter) {
    /*
     * [scan:r0 - filter:r1 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::filter {
            scalar::compare {
                    scalar::comparison_operator::equal,
                    scalar::variable_reference { c1 },
                    constant(),
            },
    });
    auto&& ro = p0.operators().insert(relation::emit {
            c2,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    // scan - p0
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1p0 = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& c2p0 = r0.columns()[1].destination();

    // project
    ASSERT_EQ(r1.condition(), (scalar::compare {
            scalar::comparison_operator::equal,
            scalar::variable_reference { c1p0 },
            constant(),
    }));

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2p0);
}

TEST_F(rewrite_stream_variables_test, buffer) {
    /*
     * [scan:r0 - buffer:r1 - emit:ro0]:p0
     *                  \
     *                   emit:ro1]
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::buffer { 2 });
    auto&& ro0 = p0.operators().insert(relation::emit {
            c1,
    });
    auto&& ro1 = p0.operators().insert(relation::emit {
            c2,
    });
    r0.output() >> r1.input();
    r1.output_ports()[0] >> ro0.input();
    r1.output_ports()[1] >> ro1.input();

    apply(p);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1p0 = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& c2p0 = r0.columns()[1].destination();

    // emit - o0
    ASSERT_EQ(ro0.columns().size(), 1);
    EXPECT_EQ(ro0.columns()[0].source(), c1p0);

    // emit - o1
    ASSERT_EQ(ro1.columns().size(), 1);
    EXPECT_EQ(ro1.columns()[0].source(), c2p0);
}

TEST_F(rewrite_stream_variables_test, emit) {
    /*
     * [scan:r0 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::emit {
            c1,
    });
    r0.output() >> r1.input();

    apply(p);

    // scan
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1p0 = r0.columns()[0].destination();

    // emit
    ASSERT_EQ(r1.columns().size(), 1);
    EXPECT_EQ(r1.columns()[0].source(), c1p0);
}

TEST_F(rewrite_stream_variables_test, write) {
    /*
     * [scan:r0 - write:r1]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::write {
            relation::write_kind::update,
            bindings(*i1),
            {
                    { c0, t1c0, },
            },
            {
                    { c2, t1c2, },
            },
    });
    r0.output() >> r1.input();

    apply(p);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& c0p0 = r0.columns()[0].destination();
    auto&& c2p0 = r0.columns()[1].destination();

    // write
    ASSERT_EQ(r1.keys().size(), 1);
    EXPECT_EQ(r1.keys()[0].source(), c0p0);
    EXPECT_EQ(r1.keys()[0].destination(), t1c0);

    ASSERT_EQ(r1.columns().size(), 1);
    EXPECT_EQ(r1.columns()[0].source(), c2p0);
    EXPECT_EQ(r1.columns()[0].destination(), t1c2);
}

TEST_F(rewrite_stream_variables_test, escape) {
    /*
     * [scan:r0 - escape:r1 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
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
    auto&& r1 = p0.operators().insert(relation::intermediate::escape {
            { c0, x0 },
            { c1, x1 },
            { c2, x2 },
    });
    auto&& ro = p0.operators().insert(relation::emit {
            x1,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    // escape operators must be deleted
    ASSERT_EQ(p0.operators().size(), 2);
    ASSERT_TRUE(p0.operators().contains(r0));
    ASSERT_TRUE(p0.operators().contains(ro));
    EXPECT_GT(r0.output(), ro.input());

    // scan - p0
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1p0 = r0.columns()[0].destination();

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c1p0);
}

TEST_F(rewrite_stream_variables_test, escape_chain) {
    /*
     * [scan:r0 - escape:r1 - escape:r2 - escape:r3 - emit:ro]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
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
    auto&& r1 = p0.operators().insert(relation::intermediate::escape {
            { c0, x0 },
            { c1, x1 },
            { c2, x2 },
    });
    auto y0 = bindings.stream_variable("y0");
    auto y1 = bindings.stream_variable("y1");
    auto y2 = bindings.stream_variable("y2");
    auto&& r2 = p0.operators().insert(relation::intermediate::escape {
            { x0, y0 },
            { x1, y1 },
            { x2, y2 },
    });
    auto z0 = bindings.stream_variable("z0");
    auto z1 = bindings.stream_variable("z1");
    auto z2 = bindings.stream_variable("z2");
    auto&& r3 = p0.operators().insert(relation::intermediate::escape {
            { y0, z0 },
            { y1, z1 },
            { y2, z2 },
    });
    auto&& ro = p0.operators().insert(relation::emit {
            z1,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> ro.input();

    apply(p);

    // escape operators must be deleted
    ASSERT_EQ(p0.operators().size(), 2);
    ASSERT_TRUE(p0.operators().contains(r0));
    ASSERT_TRUE(p0.operators().contains(ro));
    EXPECT_GT(r0.output(), ro.input());

    ASSERT_GT(r0.output(), ro.input());

    // scan
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& c1p0 = r0.columns()[0].destination();

    // emit
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c1p0);
}

TEST_F(rewrite_stream_variables_test, join_group) {
    /*
     * [scan:rl0 - offer:rl1]:pl - [group]:el -\
     *                                         [take_cogroup:rj0 - join_group:rj1 - emit:rjo]:pj
     * [scan:rr0 - offer:rr1]:pr - [group]:er -/
     */
    plan::graph_type p;

    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto&& pl = p.insert(plan::process {});
    auto&& el = p.insert(plan::group {
            {},
            { cl0 },
    });
    auto&& rl0 = pl.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto&& rl1 = pl.operators().insert(offer {
            bindings(el),
    });
    rl0.output() >> rl1.input();

    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& pr = p.insert(plan::process {});
    auto&& er = p.insert(plan::group {
            {},
            { cr0, },
    });
    auto&& rr0 = pr.operators().insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto&& rr1 = pr.operators().insert(offer {
            bindings(er),
    });
    rr0.output() >> rr1.input();

    auto&& pj = p.insert(plan::process {});
    auto&& rj0 = pj.operators().insert(take_cogroup {
            { bindings(el) },
            { bindings(er) },
    });
    auto&& rj1 = pj.operators().insert(relation::step::join {
            relation::join_kind::inner,
            scalar::compare {
                    scalar::comparison_operator::less,
                    scalar::variable_reference { cl1, },
                    constant(1),
            },
    });
    auto&& rjo = pj.operators().insert(relation::emit {
            cr2,
    });
    rj0.output() >> rj1.input();
    rj1.output() >> rjo.input();

    apply(p);

    // el
    ASSERT_EQ(el.columns().size(), 2);
    auto&& cl0el = el.columns()[0];
    auto&& cl1el = el.columns()[1];

    // er
    ASSERT_EQ(er.columns().size(), 2);
    auto&& cr0er = er.columns()[0];
    auto&& cr2er = er.columns()[1];

    // scan - pl
    ASSERT_EQ(rl0.columns().size(), 2);
    EXPECT_EQ(rl0.columns()[0].source(), t0c0);
    EXPECT_EQ(rl0.columns()[1].source(), t0c1);
    auto&& cl0pl = rl0.columns()[0].destination();
    auto&& cl1pl = rl0.columns()[1].destination();

    // offer - pl
    ASSERT_EQ(rl1.columns().size(), 2);
    EXPECT_EQ(rl1.columns()[0].source(), cl0pl);
    EXPECT_EQ(rl1.columns()[1].source(), cl1pl);
    EXPECT_EQ(rl1.columns()[0].destination(), cl0el);
    EXPECT_EQ(rl1.columns()[1].destination(), cl1el);

    // scan - pl
    ASSERT_EQ(rr0.columns().size(), 2);
    EXPECT_EQ(rr0.columns()[0].source(), t1c0);
    EXPECT_EQ(rr0.columns()[1].source(), t1c2);
    auto&& cr0pr = rr0.columns()[0].destination();
    auto&& cr2pr = rr0.columns()[1].destination();

    // offer - pl
    ASSERT_EQ(rr1.columns().size(), 2);
    EXPECT_EQ(rr1.columns()[0].source(), cr0pr);
    EXPECT_EQ(rr1.columns()[1].source(), cr2pr);
    EXPECT_EQ(rr1.columns()[0].destination(), cr0er);
    EXPECT_EQ(rr1.columns()[1].destination(), cr2er);

    // take_cogroup
    ASSERT_EQ(rj0.groups().size(), 2);
    auto&& gl = rj0.groups()[0];
    auto&& gr = rj0.groups()[1];

    ASSERT_EQ(gl.columns().size(), 1);
    EXPECT_EQ(gl.columns()[0].source(), cl1el);
    auto&& cl1pj = gl.columns()[0].destination();

    ASSERT_EQ(gr.columns().size(), 1);
    EXPECT_EQ(gr.columns()[0].source(), cr2er);
    auto&& cr2pj = gr.columns()[0].destination();

    // join_group
    EXPECT_EQ(rj1.condition(), (scalar::compare {
            scalar::comparison_operator::less,
            scalar::variable_reference { cl1pj },
            constant(1),
    }));

    ASSERT_EQ(rjo.columns().size(), 1);
    EXPECT_EQ(rjo.columns()[0].source(), cr2pj);
}

TEST_F(rewrite_stream_variables_test, aggregate_group) {
    /*
     * [scan:r0 - offer:r1]:p0 - [group]:e0 - [take_group:r2 - aggregate:r3 - emit:ro]:p1
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");

    auto&& e0 = p.insert(plan::group {
            {},
            { c0 },
    });

    auto&& p0 = p.insert(plan::process {});
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    r0.output() >> r1.input();

    auto&& p1 = p.insert(plan::process {});
    auto& r2 = p1.operators().insert(take_group {
            bindings(e0),
    });
    auto x1 = bindings.stream_variable("c3");
    auto x2 = bindings.stream_variable("c3");
    auto& r3 = p1.operators().insert(relation::step::aggregate {
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
    });
    auto& ro = p1.operators().insert(relation::emit {
            c0,
            x2,
    });
    r2.output() >> r3.input();
    r3.output() >> ro.input();

    apply(p);

    ASSERT_EQ(e0.columns().size(), 2);
    auto&& c0e0 = e0.columns()[0];
    auto&& c2e0 = e0.columns()[1];

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& c0p0 = r0.columns()[0].destination();
    auto&& c2p0 = r0.columns()[1].destination();

    // offer
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), c0p0);
    EXPECT_EQ(r1.columns()[1].source(), c2p0);
    EXPECT_EQ(r1.columns()[0].destination(), c0e0);
    EXPECT_EQ(r1.columns()[1].destination(), c2e0);

    // take_group
    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0].source(), c0e0);
    EXPECT_EQ(r2.columns()[1].source(), c2e0);
    auto&& c0p1 = r2.columns()[0].destination();
    auto&& c2p1 = r2.columns()[1].destination();

    // aggregate
    ASSERT_EQ(r3.columns().size(), 1);
    auto&& a0 = r3.columns()[0];
    ASSERT_EQ(a0.arguments().size(), 1);
    EXPECT_EQ(a0.arguments()[0], c2p1);
    auto&& x2p1 = a0.destination();

    // offer
    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), c0p1);
    EXPECT_EQ(ro.columns()[1].source(), x2p1);
}

TEST_F(rewrite_stream_variables_test, intersection_group) {
    /*
     * [scan:rl0 - offer:rl1]:pl - [group]:el -\
     *                                         [take_cogroup:rj0 - intersection:rj1 - emit:rjo]:pj
     * [scan:rr0 - offer:rr1]:pr - [group]:er -/
     */
    plan::graph_type p;

    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto&& pl = p.insert(plan::process {});
    auto&& el = p.insert(plan::group {
            {},
            { cl0 },
    });
    auto&& rl0 = pl.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto&& rl1 = pl.operators().insert(offer {
            bindings(el),
    });
    rl0.output() >> rl1.input();

    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& pr = p.insert(plan::process {});
    auto&& er = p.insert(plan::group {
            {},
            { cr0, },
    });
    auto&& rr0 = pr.operators().insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto&& rr1 = pr.operators().insert(offer {
            bindings(er),
    });
    rr0.output() >> rr1.input();

    auto&& pj = p.insert(plan::process {});
    auto&& rj0 = pj.operators().insert(take_cogroup {
            { bindings(el) },
            { bindings(er) },
    });
    auto&& rj1 = pj.operators().insert(relation::step::intersection {});
    auto&& rjo = pj.operators().insert(relation::emit {
            cl2,
    });
    rj0.output() >> rj1.input();
    rj1.output() >> rjo.input();

    apply(p);

    // el
    ASSERT_EQ(el.columns().size(), 2);
    auto&& cl0el = el.columns()[0];
    auto&& cl2el = el.columns()[1];

    // er
    ASSERT_EQ(er.columns().size(), 1);
    auto&& cr0er = er.columns()[0];

    // scan - pl
    ASSERT_EQ(rl0.columns().size(), 2);
    EXPECT_EQ(rl0.columns()[0].source(), t0c0);
    EXPECT_EQ(rl0.columns()[1].source(), t0c2);
    auto&& cl0pl = rl0.columns()[0].destination();
    auto&& cl2pl = rl0.columns()[1].destination();

    // offer - pl
    ASSERT_EQ(rl1.columns().size(), 2);
    EXPECT_EQ(rl1.columns()[0].source(), cl0pl);
    EXPECT_EQ(rl1.columns()[1].source(), cl2pl);
    EXPECT_EQ(rl1.columns()[0].destination(), cl0el);
    EXPECT_EQ(rl1.columns()[1].destination(), cl2el);

    // scan - pl
    ASSERT_EQ(rr0.columns().size(), 1);
    EXPECT_EQ(rr0.columns()[0].source(), t1c0);
    auto&& cr0pr = rr0.columns()[0].destination();

    // offer - pl
    ASSERT_EQ(rr1.columns().size(), 1);
    EXPECT_EQ(rr1.columns()[0].source(), cr0pr);
    EXPECT_EQ(rr1.columns()[0].destination(), cr0er);

    // take_cogroup
    ASSERT_EQ(rj0.groups().size(), 2);
    auto&& gl = rj0.groups()[0];
    auto&& gr = rj0.groups()[1];

    ASSERT_EQ(gl.columns().size(), 1);
    EXPECT_EQ(gl.columns()[0].source(), cl2el);
    auto&& cl2pj = gl.columns()[0].destination();

    ASSERT_EQ(gr.columns().size(), 0); // is ok?

    // intersection

    // offer
    ASSERT_EQ(rjo.columns().size(), 1);
    EXPECT_EQ(rjo.columns()[0].source(), cl2pj);
}

TEST_F(rewrite_stream_variables_test, difference_group) {
    /*
     * [scan:rl0 - offer:rl1]:pl - [group]:el -\
     *                                         [take_cogroup:rj0 - difference:rj1 - emit:rjo]:pj
     * [scan:rr0 - offer:rr1]:pr - [group]:er -/
     */
    plan::graph_type p;

    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto&& pl = p.insert(plan::process {});
    auto&& el = p.insert(plan::group {
            {},
            { cl0 },
    });
    auto&& rl0 = pl.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto&& rl1 = pl.operators().insert(offer {
            bindings(el),
    });
    rl0.output() >> rl1.input();

    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& pr = p.insert(plan::process {});
    auto&& er = p.insert(plan::group {
            {},
            { cr0, },
    });
    auto&& rr0 = pr.operators().insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto&& rr1 = pr.operators().insert(offer {
            bindings(er),
    });
    rr0.output() >> rr1.input();

    auto&& pj = p.insert(plan::process {});
    auto&& rj0 = pj.operators().insert(take_cogroup {
            { bindings(el) },
            { bindings(er) },
    });
    auto&& rj1 = pj.operators().insert(relation::step::difference {});
    auto&& rjo = pj.operators().insert(relation::emit {
            cl2,
    });
    rj0.output() >> rj1.input();
    rj1.output() >> rjo.input();

    apply(p);

    // el
    ASSERT_EQ(el.columns().size(), 2);
    auto&& cl0el = el.columns()[0];
    auto&& cl2el = el.columns()[1];

    // er
    ASSERT_EQ(er.columns().size(), 1);
    auto&& cr0er = er.columns()[0];

    // scan - pl
    ASSERT_EQ(rl0.columns().size(), 2);
    EXPECT_EQ(rl0.columns()[0].source(), t0c0);
    EXPECT_EQ(rl0.columns()[1].source(), t0c2);
    auto&& cl0pl = rl0.columns()[0].destination();
    auto&& cl2pl = rl0.columns()[1].destination();

    // offer - pl
    ASSERT_EQ(rl1.columns().size(), 2);
    EXPECT_EQ(rl1.columns()[0].source(), cl0pl);
    EXPECT_EQ(rl1.columns()[1].source(), cl2pl);
    EXPECT_EQ(rl1.columns()[0].destination(), cl0el);
    EXPECT_EQ(rl1.columns()[1].destination(), cl2el);

    // scan - pl
    ASSERT_EQ(rr0.columns().size(), 1);
    EXPECT_EQ(rr0.columns()[0].source(), t1c0);
    auto&& cr0pr = rr0.columns()[0].destination();

    // offer - pl
    ASSERT_EQ(rr1.columns().size(), 1);
    EXPECT_EQ(rr1.columns()[0].source(), cr0pr);
    EXPECT_EQ(rr1.columns()[0].destination(), cr0er);

    // take_cogroup
    ASSERT_EQ(rj0.groups().size(), 2);
    auto&& gl = rj0.groups()[0];
    auto&& gr = rj0.groups()[1];

    ASSERT_EQ(gl.columns().size(), 1);
    EXPECT_EQ(gl.columns()[0].source(), cl2el);
    auto&& cl2pj = gl.columns()[0].destination();

    ASSERT_EQ(gr.columns().size(), 0); // is ok?

    // intersection

    // offer
    ASSERT_EQ(rjo.columns().size(), 1);
    EXPECT_EQ(rjo.columns()[0].source(), cl2pj);
}

TEST_F(rewrite_stream_variables_test, flatten_group) {
    /*
     * [scan:r0 - offer:r1]:p0 - [group]:e0 - [take_group:r2 - flatten:r3 - emit:ro]:p1
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");

    auto&& e0 = p.insert(plan::group {
            {},
            { c0 },
    });

    auto&& p0 = p.insert(plan::process {});
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    r0.output() >> r1.input();

    auto&& p1 = p.insert(plan::process {});
    auto& r2 = p1.operators().insert(take_group {
            bindings(e0),
    });
    auto& r3 = p1.operators().insert(relation::step::flatten {});
    auto& ro = p1.operators().insert(relation::emit {
            c2,
    });
    r2.output() >> r3.input();
    r3.output() >> ro.input();

    apply(p);

    ASSERT_EQ(e0.columns().size(), 2);
    auto&& c0e0 = e0.columns()[0];
    auto&& c2e0 = e0.columns()[1];

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& c0p0 = r0.columns()[0].destination();
    auto&& c2p0 = r0.columns()[1].destination();

    // offer
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), c0p0);
    EXPECT_EQ(r1.columns()[1].source(), c2p0);
    EXPECT_EQ(r1.columns()[0].destination(), c0e0);
    EXPECT_EQ(r1.columns()[1].destination(), c2e0);

    // take_group
    ASSERT_EQ(r2.columns().size(), 1);
    EXPECT_EQ(r2.columns()[0].source(), c2e0);
    auto&& c2p1 = r2.columns()[0].destination();

    // flatten

    // offer
    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2p1);
}

TEST_F(rewrite_stream_variables_test, forward) {
    /*
     * [scan:r0 - offer:r1]:p0 - [forward]:e0 - [take_flat:r2 - emit:r3]:p1
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    auto&& e0 = p.insert(plan::forward {});

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    auto&& r2 = p1.operators().insert(take_flat {
            bindings(e0),
    });
    auto&& r3 = p1.operators().insert(relation::emit {
            c1,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.input();

    apply(p);
    ASSERT_EQ(e0.columns().size(), 1);
    auto&& e0c1 = e0.columns()[0];

    // scan
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    auto&& p0c1 = r0.columns()[0].destination();

    // offer
    ASSERT_EQ(r1.columns().size(), 1);
    EXPECT_EQ(r1.columns()[0].source(), p0c1);
    EXPECT_EQ(r1.columns()[0].destination(), e0c1);

    // take
    ASSERT_EQ(r2.columns().size(), 1);
    EXPECT_EQ(r2.columns()[0].source(), e0c1);
    auto&& p1c1 = r2.columns()[0].destination();

    // emit
    ASSERT_EQ(r3.columns().size(), 1);
    EXPECT_EQ(r3.columns()[0].source(), p1c1);
}

TEST_F(rewrite_stream_variables_test, group) {
    /*
     * [scan:r0 - offer:r1]:p0 - [group]:e0 - [take_group:r2 - flatten:r3 - emit:r4]:p1
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    auto&& e0 = p.insert(plan::group {
            {},
            { c0 },
    });

    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    auto&& r2 = p1.operators().insert(take_group {
            bindings(e0),
    });
    auto&& r3 = p1.operators().insert(flatten {});
    auto&& r4 = p1.operators().insert(relation::emit {
            c2,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(p);

    ASSERT_EQ(e0.columns().size(), 2);
    auto&& e0c0 = e0.columns()[0];
    auto&& e0c2 = e0.columns()[1];

    ASSERT_EQ(e0.group_keys().size(), 1);
    EXPECT_EQ(e0.group_keys()[0], e0c0);

    ASSERT_EQ(e0.sort_keys().size(), 0);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    auto&& p0c0 = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& p0c2 = r0.columns()[1].destination();

    // offer
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), p0c0);
    EXPECT_EQ(r1.columns()[0].destination(), e0c0);
    EXPECT_EQ(r1.columns()[1].source(), p0c2);
    EXPECT_EQ(r1.columns()[1].destination(), e0c2);

    // take
    ASSERT_EQ(r2.columns().size(), 1);
    EXPECT_EQ(r2.columns()[0].source(), e0c2);
    auto&& p1c2 = r2.columns()[0].destination();

    // emit
    ASSERT_EQ(r4.columns().size(), 1);
    EXPECT_EQ(r4.columns()[0].source(), p1c2);
}

TEST_F(rewrite_stream_variables_test, aggregate) {
    /*
     * [scan:r0 - offer:r1]:p0 - [aggregate]:e0 - [take_group:r2 - flatten:r3 - emit:r4]:p1
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    // group be c0, aggregate agg0(c1) as a0, agg1(c2) as a1
    auto&& e0 = p.insert(plan::aggregate {
            {},
            {},
            { c0, },
            {
                    {
                            bindings(agg0),
                            { c1, },
                            a0,
                    },
                    {
                            bindings(agg0),
                            { c2, },
                            a1,
                    },
            },
    });

    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    auto&& r2 = p1.operators().insert(take_group {
            bindings(e0),
    });
    auto&& r3 = p1.operators().insert(flatten {});
    auto&& r4 = p1.operators().insert(relation::emit {
            a1,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(p);

    ASSERT_EQ(e0.source_columns().size(), 2);
    auto&& e0c0 = e0.source_columns()[0];
    auto&& e0c2 = e0.source_columns()[1];

    ASSERT_EQ(e0.destination_columns().size(), 2);
    EXPECT_EQ(e0.destination_columns()[0], e0c0); // always keep group key
    auto&& e0a1 = e0.destination_columns()[1];

    ASSERT_EQ(e0.group_keys().size(), 1);
    EXPECT_EQ(e0.group_keys()[0], e0c0);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    auto&& p0c0 = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& p0c2 = r0.columns()[1].destination();

    // offer
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), p0c0);
    EXPECT_EQ(r1.columns()[0].destination(), e0c0);
    EXPECT_EQ(r1.columns()[1].source(), p0c2);
    EXPECT_EQ(r1.columns()[1].destination(), e0c2);

    // take
    ASSERT_EQ(r2.columns().size(), 1);
    EXPECT_EQ(r2.columns()[0].source(), e0a1);
    auto&& p1c2 = r2.columns()[0].destination();

    // emit
    ASSERT_EQ(r4.columns().size(), 1);
    EXPECT_EQ(r4.columns()[0].source(), p1c2);
}

TEST_F(rewrite_stream_variables_test, broadcast) {
    /*
     * [scan:r0 - offer:r1]:p0 - [broadcast]:e0 -
     *                                           \
     * [scan:r2 --------------------------------- join_find:r3 - emit:r4]:p1
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    auto&& e0 = p.insert(plan::broadcast {});

    auto p0c0 = bindings.stream_variable("p0c0");
    auto p0c1 = bindings.stream_variable("p0c1");
    auto p0c2 = bindings.stream_variable("p0c2");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, p0c0 },
                    { t0c1, p0c1 },
                    { t0c2, p0c2 },
            },
    });
    auto&& r1 = p0.operators().insert(offer {
            bindings(e0),
    });

    auto p1c0 = bindings.stream_variable("p1c0");
    auto p1c1 = bindings.stream_variable("p1c1");
    auto p1c2 = bindings.stream_variable("p1c2");
    auto&& r2 = p1.operators().insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, p1c0 },
                    { t1c1, p1c1 },
                    { t1c2, p1c2 },
            },
    });

    auto&& r3 = p1.operators().insert(relation::join_find {
            relation::join_kind::inner,
            bindings(e0),
            {},
            {
                    relation::join_find::key {
                            p0c0,
                            scalar::variable_reference { p1c0 },
                    },
            },
    });

    auto&& r4 = p1.operators().insert(relation::emit {
            p0c2,
            p1c2,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.left();
    r3.output() >> r4.input();

    apply(p);

    ASSERT_EQ(e0.columns().size(), 2);
    auto&& e0c0 = e0.columns()[0];
    auto&& e0c2 = e0.columns()[1];

    // p0::scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    auto&& p0c0m = r0.columns()[0].destination();
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    auto&& p0c2m = r0.columns()[1].destination();

    // p0::offer
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), p0c0m);
    EXPECT_EQ(r1.columns()[0].destination(), e0c0);
    EXPECT_EQ(r1.columns()[1].source(), p0c2m);
    EXPECT_EQ(r1.columns()[1].destination(), e0c2);

    // p1::scan
    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0].source(), t1c0);
    auto&& p1c0m = r2.columns()[0].destination();
    EXPECT_EQ(r2.columns()[1].source(), t1c2);
    auto&& p1c2m = r2.columns()[1].destination();

    // join _find
    ASSERT_EQ(r3.columns().size(), 1);
    EXPECT_EQ(r3.columns()[0].source(), e0c2);
    auto&& p0c2m2 = r3.columns()[0].destination();

    ASSERT_EQ(r3.keys().size(), 1);
    EXPECT_EQ(r3.keys()[0].variable(), e0c0);
    EXPECT_EQ(r3.keys()[0].value(), scalar::variable_reference(p1c0m));

    // emit
    ASSERT_EQ(r4.columns().size(), 2);
    EXPECT_EQ(r4.columns()[0].source(), p0c2m2);
    EXPECT_EQ(r4.columns()[1].source(), p1c2m);
}

} // namespace yugawara::analyzer::details
