#include <yugawara/analyzer/details/remove_redundant_stream_variables.h>

#include <gtest/gtest.h>

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

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class remove_redundant_stream_variables_test : public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    type::repository types;
    binding::factory bindings;

    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table("t0", {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::table> t1 = storages.add_table("t1", {
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

    std::shared_ptr<storage::index> i0 = storages.add_index("I0", { t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages.add_index("I1", { t1, "I1" });

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

    void apply(relation::graph_type& graph) {
        details::remove_redundant_stream_variables(graph, creator);
    }
};

TEST_F(remove_redundant_stream_variables_test, find) {
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
                    relation::find::key { t0c0, constant() },
            },
    });
    auto&& ro = r.insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[0].destination(), c1);
}

TEST_F(remove_redundant_stream_variables_test, scan) {
    /*
     * scan:r0 - emit:ro
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
    auto&& ro = r.insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[0].destination(), c1);
}

TEST_F(remove_redundant_stream_variables_test, join_relation) {
    /*
     *  scan:rl - join_relation:r0 - emit:ro
     *           /
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
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });
    auto&& ro = r.insert(relation::emit {
            cl1,
            cr2,
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply(r);

    // scan - left
    ASSERT_EQ(rl.columns().size(), 2);
    EXPECT_EQ(rl.columns()[0].source(), t0c0);
    EXPECT_EQ(rl.columns()[0].destination(), cl0);
    EXPECT_EQ(rl.columns()[1].source(), t0c1);
    EXPECT_EQ(rl.columns()[1].destination(), cl1);

    // scan - right
    ASSERT_EQ(rr.columns().size(), 2);
    EXPECT_EQ(rr.columns()[0].source(), t1c0);
    EXPECT_EQ(rr.columns()[0].destination(), cr0);
    EXPECT_EQ(rr.columns()[1].source(), t1c2);
    EXPECT_EQ(rr.columns()[1].destination(), cr2);
}

TEST_F(remove_redundant_stream_variables_test, join_find) {
    /*
     * scan:r0 - join_find:r1 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = r.insert(relation::join_find {
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
    auto&& ro = r.insert(relation::emit {
            j2,
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    EXPECT_EQ(r0.columns()[1].destination(), c1);

    // join_find
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), t1c1);
    EXPECT_EQ(r1.columns()[0].destination(), j1);
    EXPECT_EQ(r1.columns()[1].source(), t1c2);
    EXPECT_EQ(r1.columns()[1].destination(), j2);

    ASSERT_EQ(r1.keys().size(), 1);
    EXPECT_EQ(r1.keys()[0].variable(), t1c0);
    EXPECT_EQ(r1.keys()[0].value(), scalar::variable_reference(c0));

    EXPECT_EQ(r1.condition(), compare(c1, j1));
}

TEST_F(remove_redundant_stream_variables_test, join_scan) {
    /*
     * scan:r0 - join_scan:r1 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = r.insert(relation::join_scan {
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
    auto&& ro = r.insert(relation::emit {
            j2,
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    EXPECT_EQ(r0.columns()[1].destination(), c1);

    // join_scan
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), t1c1);
    EXPECT_EQ(r1.columns()[0].destination(), j1);
    EXPECT_EQ(r1.columns()[1].source(), t1c2);
    EXPECT_EQ(r1.columns()[1].destination(), j2);

    ASSERT_EQ(r1.lower().keys().size(), 1);
    EXPECT_EQ(r1.lower().keys()[0].variable(), t1c0);
    EXPECT_EQ(r1.lower().keys()[0].value(), scalar::variable_reference(c0));

    EXPECT_EQ(r1.condition(), compare(c1, j1));
}

TEST_F(remove_redundant_stream_variables_test, project) {
    /*
     * scan:r0 - project:r1 - emit:ro
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
    auto x1 = bindings.stream_variable("x0");
    auto x2 = bindings.stream_variable("x0");
    auto x3 = bindings.stream_variable("x3");
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
    auto&& ro = r.insert(relation::emit {
            x2,
            x3,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c1);
    EXPECT_EQ(r0.columns()[1].destination(), c2);

    // project
    ASSERT_EQ(r1.columns().size(), 3);
    EXPECT_EQ(r1.columns()[0].variable(), x1);
    EXPECT_EQ(r1.columns()[1].variable(), x2);
    EXPECT_EQ(r1.columns()[2].variable(), x3);
    EXPECT_EQ(r1.columns()[0].value(), (scalar::binary {
            scalar::binary_operator::add,
            scalar::variable_reference { c1 },
            constant(2),
    }));
    EXPECT_EQ(r1.columns()[1].value(), (scalar::binary {
            scalar::binary_operator::add,
            scalar::variable_reference { c2 },
            scalar::variable_reference { x1 },
    }));
    EXPECT_EQ(r1.columns()[2].value(), constant(3));
}

TEST_F(remove_redundant_stream_variables_test, filter) {
    /*
     * scan:r0 - filter:r1 - emit:ro
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
    auto&& r1 = r.insert(relation::filter {
            scalar::compare {
                    scalar::comparison_operator::equal,
                    scalar::variable_reference { c1 },
                    constant(),
            },
    });
    auto&& ro = r.insert(relation::emit {
            c2,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c1);
    EXPECT_EQ(r0.columns()[1].destination(), c2);

    // project
    ASSERT_EQ(r1.condition(), (scalar::compare {
            scalar::comparison_operator::equal,
            scalar::variable_reference { c1 },
            constant(),
    }));
}

TEST_F(remove_redundant_stream_variables_test, buffer) {
    /*
     * scan:r0 - buffer:r1 - emit:ro0
     *                 \
     *                  emit:ro1
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
    auto&& ro0 = r.insert(relation::emit {
            c1,
    });
    auto&& ro1 = r.insert(relation::emit {
            c2,
    });
    r0.output() >> r1.input();
    r1.output_ports()[0] >> ro0.input();
    r1.output_ports()[1] >> ro1.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c1);
    EXPECT_EQ(r0.columns()[1].destination(), c2);
}

TEST_F(remove_redundant_stream_variables_test, aggregate_relation) {
    /*
     * scan:r0 - aggregate_relation:r1 - emit:ro
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
    auto& ro = r.insert(relation::emit {
            x2,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c2);

    // aggregate
    ASSERT_EQ(r1.columns().size(), 1);
    auto&& a0 = r1.columns()[0];
    ASSERT_EQ(a0.arguments().size(), 1);
    EXPECT_EQ(a0.arguments()[0], c2);
    EXPECT_EQ(a0.destination(), x2);
}

TEST_F(remove_redundant_stream_variables_test, aggregate_relation_key_only) {
    /*
     * scan:r0 - aggregate_relation:r1 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan{
            bindings(*i0),
            {
                    {t0c0, c0},
                    {t0c1, c1},
                    {t0c2, c2},
            },
    });
    auto& r1 = r.insert(relation::intermediate::aggregate{
            {
                    c0,
            },
            {},
    });
    auto& ro = r.insert(relation::emit{
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[0].destination(), c0);

    // aggregate
    ASSERT_EQ(r1.columns().size(), 0);
    ASSERT_EQ(r1.group_keys().size(), 1);
    EXPECT_EQ(r1.group_keys()[0], c0);
}

TEST_F(remove_redundant_stream_variables_test, distinct_relation) {
    /*
     * scan:r0 - distinct_relation:r1 - emit:ro
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
    auto& ro = r.insert(relation::emit {
            c2,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c2);

    // distinct
    ASSERT_EQ(r1.group_keys().size(), 1);
    EXPECT_EQ(r1.group_keys()[0], c0);
}

TEST_F(remove_redundant_stream_variables_test, limit_relation) {
    /*
     * scan:r0 - limit_relation:r1 - emit:ro
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
    auto& ro = r.insert(relation::emit {
            c2,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    EXPECT_EQ(r0.columns()[2].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c1);
    EXPECT_EQ(r0.columns()[2].destination(), c2);

    // limit
    ASSERT_EQ(r1.group_keys().size(), 1);
    EXPECT_EQ(r1.group_keys()[0], c0);
    ASSERT_EQ(r1.sort_keys().size(), 1);
    EXPECT_EQ(r1.sort_keys()[0].variable(), c1);
}

TEST_F(remove_redundant_stream_variables_test, intersection_relation) {
    /*
     * scan:rl -\
     *           intersection_relation:r0 - emit:ro
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
    auto&& ro = r.insert(relation::emit {
            cl1,
            cr2,
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply(r);

    // scan - left
    ASSERT_EQ(rl.columns().size(), 2);
    EXPECT_EQ(rl.columns()[0].source(), t0c0);
    EXPECT_EQ(rl.columns()[0].destination(), cl0);
    EXPECT_EQ(rl.columns()[1].source(), t0c1);
    EXPECT_EQ(rl.columns()[1].destination(), cl1);

    // scan - right
    ASSERT_EQ(rr.columns().size(), 2);
    EXPECT_EQ(rr.columns()[0].source(), t1c0);
    EXPECT_EQ(rr.columns()[0].destination(), cr0);
    EXPECT_EQ(rr.columns()[1].source(), t1c2);
    EXPECT_EQ(rr.columns()[1].destination(), cr2);
}

TEST_F(remove_redundant_stream_variables_test, difference_relation) {
    /*
     * scan:rl -\
     *           difference_relation:r0 - emit:ro
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
    auto&& ro = r.insert(relation::emit {
            cl1,
            cr2,
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply(r);

    // scan - left
    ASSERT_EQ(rl.columns().size(), 2);
    EXPECT_EQ(rl.columns()[0].source(), t0c0);
    EXPECT_EQ(rl.columns()[0].destination(), cl0);
    EXPECT_EQ(rl.columns()[1].source(), t0c1);
    EXPECT_EQ(rl.columns()[1].destination(), cl1);

    // scan - right
    ASSERT_EQ(rr.columns().size(), 2);
    EXPECT_EQ(rr.columns()[0].source(), t1c0);
    EXPECT_EQ(rr.columns()[0].destination(), cr0);
    EXPECT_EQ(rr.columns()[1].source(), t1c2);
    EXPECT_EQ(rr.columns()[1].destination(), cr2);
}

TEST_F(remove_redundant_stream_variables_test, emit) {
    /*
     * scan:r0 - emit:ro
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
    auto&& r1 = r.insert(relation::emit {
            c1,
    });
    r0.output() >> r1.input();

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[0].destination(), c1);

    // emit
    ASSERT_EQ(r1.columns().size(), 1);
    EXPECT_EQ(r1.columns()[0].source(), c1);
}

TEST_F(remove_redundant_stream_variables_test, write) {
    /*
     * scan:r0 - write:r1
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
    auto&& r1 = r.insert(relation::write {
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

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 2);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c2);

    // write
    ASSERT_EQ(r1.keys().size(), 1);
    EXPECT_EQ(r1.keys()[0].source(), c0);
    EXPECT_EQ(r1.keys()[0].destination(), t1c0);

    ASSERT_EQ(r1.columns().size(), 1);
    EXPECT_EQ(r1.columns()[0].source(), c2);
    EXPECT_EQ(r1.columns()[0].destination(), t1c2);
}

TEST_F(remove_redundant_stream_variables_test, escape) {
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

    apply(r);

    // scan
    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0].source(), t0c1);
    EXPECT_EQ(r0.columns()[0].destination(), c1);

    // escape
    ASSERT_EQ(r1.mappings().size(), 1);
    EXPECT_EQ(r1.mappings()[0].source(), c1);
    EXPECT_EQ(r1.mappings()[0].destination(), x1);
}

} // namespace yugawara::analyzer::details
