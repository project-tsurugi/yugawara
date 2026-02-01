#include <yugawara/analyzer/details/remove_variable_aliases.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/identify.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>

#include <yugawara/binding/factory.h>
#include <yugawara/aggregate/declaration.h>
#include <yugawara/extension/relation/subquery.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class remove_variable_aliases_test : public ::testing::Test {
protected:
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

    std::shared_ptr<storage::index> i0 = storages.add_index({
            t0,
            "I0",
            {
                    t0->columns()[0],
            },
            {
                    t0->columns()[1],
                    t0->columns()[2],
            },
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique,
            }
    });
    std::shared_ptr<storage::index> i1 = storages.add_index({
            t1,
            "I1",
            {
                    t1->columns()[0],
            },
            {
                    t1->columns()[1],
                    t1->columns()[2],
            },
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique,
            }
    });

    void apply(relation::graph_type& graph) {
        remove_variable_aliases(graph);
        dump(graph, std::cout);
    }
};

TEST_F(remove_variable_aliases_test, find) {
    /*
     * find:r0 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::find {
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
    auto&& ro = p.insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    EXPECT_EQ(r0.columns()[2].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c1);
    EXPECT_EQ(r0.columns()[2].destination(), c2);

    ASSERT_EQ(r0.keys().size(), 1);
    EXPECT_EQ(r0.keys()[0].variable(), t0c0);
    EXPECT_EQ(r0.keys()[0].value(), constant());

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c1);
}

TEST_F(remove_variable_aliases_test, scan) {
    /*
     * scan:r0 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
            {
                    relation::scan::key { t0c0, constant() },
                    relation::endpoint_kind::inclusive,
            },
            {
                    relation::scan::key { t0c0, constant() },
                    relation::endpoint_kind::inclusive,
            },
    });
    auto&& ro = p.insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0].source(), t0c0);
    EXPECT_EQ(r0.columns()[1].source(), t0c1);
    EXPECT_EQ(r0.columns()[2].source(), t0c2);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c1);
    EXPECT_EQ(r0.columns()[2].destination(), c2);

    ASSERT_EQ(r0.lower().keys().size(), 1);
    EXPECT_EQ(r0.lower().keys()[0].variable(), t0c0);
    EXPECT_EQ(r0.lower().keys()[0].value(), constant());

    ASSERT_EQ(r0.upper().keys().size(), 1);
    EXPECT_EQ(r0.upper().keys()[0].variable(), t0c0);
    EXPECT_EQ(r0.upper().keys()[0].value(), constant());

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c1);
}

TEST_F(remove_variable_aliases_test, values) {
    /*
     * values:r0 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto&& ro = p.insert(relation::emit {
            c1,
    });
    r0.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c1);
}

TEST_F(remove_variable_aliases_test, join_find) {
    /*
     * values:r0 - project:r1 - join_find:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r2 = p.insert(relation::join_find {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t0c0, j0 },
                    { t0c1, j1 },
                    { t0c2, j2 },
            },
            {
                    relation::join_find::key { t0c0, scalar::variable_reference { a0 } }
            },
            compare(a1, j1),
    });
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.left();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.columns().size(), 3);
    EXPECT_EQ(r2.columns()[0].source(), t0c0);
    EXPECT_EQ(r2.columns()[1].source(), t0c1);
    EXPECT_EQ(r2.columns()[2].source(), t0c2);
    EXPECT_EQ(r2.columns()[0].destination(), j0);
    EXPECT_EQ(r2.columns()[1].destination(), j1);
    EXPECT_EQ(r2.columns()[2].destination(), j2);

    ASSERT_EQ(r2.keys().size(), 1);
    EXPECT_EQ(r2.keys()[0].variable(), t0c0);
    EXPECT_EQ(r2.keys()[0].value(), scalar::variable_reference(c0));

    EXPECT_EQ(r2.condition(), compare(c1, j1));

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, join_scan) {
    /*
     * values:r0 - project:r1 - join_scan:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto j2 = bindings.stream_variable("j2");
    auto&& r2 = p.insert(relation::join_scan {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t0c0, j0 },
                    { t0c1, j1 },
                    { t0c2, j2 },
            },
            {
                    relation::join_scan::key { t0c0, scalar::variable_reference { a0 } },
                    relation::endpoint_kind::inclusive,
            },
            {
                    relation::join_scan::key { t0c0, scalar::variable_reference { a1 } },
                    relation::endpoint_kind::inclusive,
            },
            compare(a2, j2),
    });
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.left();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.columns().size(), 3);
    EXPECT_EQ(r2.columns()[0].source(), t0c0);
    EXPECT_EQ(r2.columns()[1].source(), t0c1);
    EXPECT_EQ(r2.columns()[2].source(), t0c2);
    EXPECT_EQ(r2.columns()[0].destination(), j0);
    EXPECT_EQ(r2.columns()[1].destination(), j1);
    EXPECT_EQ(r2.columns()[2].destination(), j2);

    ASSERT_EQ(r2.lower().keys().size(), 1);
    EXPECT_EQ(r2.lower().keys()[0].variable(), t0c0);
    EXPECT_EQ(r2.lower().keys()[0].value(), scalar::variable_reference(c0));

    ASSERT_EQ(r2.upper().keys().size(), 1);
    EXPECT_EQ(r2.upper().keys()[0].variable(), t0c0);
    EXPECT_EQ(r2.upper().keys()[0].value(), scalar::variable_reference(c1));

    EXPECT_EQ(r2.condition(), compare(c2, j2));

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, apply) {
    /*
     * values:r0 - project:r1 - apply:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
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
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r2 = p.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { a0 },
                    scalar::variable_reference { a1 },
            },
            {
                    { 0, x0 },
                    { 1, x1 },
            },
    });
    auto&& ro = p.insert(relation::emit {
            a2,
            x1,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.arguments().size(), 2);
    EXPECT_EQ(r2.arguments()[0], scalar::variable_reference(c0));
    EXPECT_EQ(r2.arguments()[1], scalar::variable_reference(c1));

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0].position(), 0);
    EXPECT_EQ(r2.columns()[1].position(), 1);
    EXPECT_EQ(r2.columns()[0].variable(), x0);
    EXPECT_EQ(r2.columns()[1].variable(), x1);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), c2);
    EXPECT_EQ(ro.columns()[1].source(), x1);
}

TEST_F(remove_variable_aliases_test, project) {
    /*
     * values:r0 - project:r1 - project:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto&& r2 = p.insert(relation::project {
            relation::project::column {
                    x0,
                    constant(42),
            },
            relation::project::column {
                    x1,
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { a0 },
                            scalar::variable_reference { a1 },
                    },
            },
            relation::project::column {
                    x2, // just an alias
                    scalar::variable_reference { a2 },
            },
    });
    auto&& ro = p.insert(relation::emit {
            x0,
            x1,
            x2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0].variable(), x0);
    EXPECT_EQ(r2.columns()[0].value(), constant(42));
    EXPECT_EQ(r2.columns()[1].variable(), x1);
    EXPECT_EQ(r2.columns()[1].value(), (scalar::binary {
            scalar::binary_operator::add,
            scalar::variable_reference { c0 },
            scalar::variable_reference { c1 },
    }));

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), x0);
    EXPECT_EQ(ro.columns()[1].source(), x1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
}

TEST_F(remove_variable_aliases_test, filter) {
    /*
     * values:r0 - project:r1 - filter:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto&& r2 = p.insert(relation::filter {
            compare(a0, a1),
    });
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    EXPECT_EQ(r2.condition(), compare(c0, c1));

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, buffer) {
    /*
     * values:r0 - project:r1 - buffer:r2 -+-- emit:ro0
     *                                     |
     *                                     +-- emit:ro1
     *                                     |
     *                                     +-- emit:ro2
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto&& r2 = p.insert(relation::buffer { 3 });
    auto&& ro0 = p.insert(relation::emit {
            a0,
    });
    auto&& ro1 = p.insert(relation::emit {
            a1,
    });
    auto&& ro2 = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output_ports()[0] >> ro0.input();
    r2.output_ports()[1] >> ro1.input();
    r2.output_ports()[2] >> ro2.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(ro0.columns().size(), 1);
    EXPECT_EQ(ro0.columns()[0].source(), c0);

    ASSERT_EQ(ro1.columns().size(), 1);
    EXPECT_EQ(ro1.columns()[0].source(), c1);

    ASSERT_EQ(ro2.columns().size(), 1);
    EXPECT_EQ(ro2.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, identify) {
    /*
     * values:r0 - project:r1 - identify:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto x0 = bindings.stream_variable("x0");
    auto&& r2 = p.insert(relation::identify {
            x0,
            ::takatori::type::row_id { 123 },
    });
    auto&& ro = p.insert(relation::emit {
            a0,
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    EXPECT_EQ(r2.variable(), x0);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), x0);
}

TEST_F(remove_variable_aliases_test, emit) {
    /*
     * values:r0 - project:r1 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto&& ro = p.insert(relation::emit {
            a0,
            a1,
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
}

TEST_F(remove_variable_aliases_test, write) {
    /*
     * values:r0 - project:r1 - write:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto&& ro = p.insert(relation::write {
            relation::write_kind::insert,
            bindings(*i0),
            {
                    { a0, t0c0 },
            },
            {
                    { a0, t0c0 },
                    { a1, t0c1 },
                    { a2, t0c2 },
            },
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(ro.keys().size(), 1);
    EXPECT_EQ(ro.keys()[0].source(), c0);
    EXPECT_EQ(ro.keys()[0].destination(), t0c0);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
    EXPECT_EQ(ro.columns()[0].destination(), t0c0);
    EXPECT_EQ(ro.columns()[1].destination(), t0c1);
    EXPECT_EQ(ro.columns()[2].destination(), t0c2);
}

TEST_F(remove_variable_aliases_test, join_group) {
    /*
     * values:r0 - project:r1 - join_group:r3 - emit:ro
     *                         /
     *            values:r2 --/
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto&& r2 = p.insert(relation::values {
            { j0, j1 },
            {
                    { constant(100), constant(200), },
            },
    });
    auto&& r3 = p.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(a2, j1),
    });
    r3.lower() = {
            relation::intermediate::join::key { j0, scalar::variable_reference { a0 } },
            relation::endpoint_kind::inclusive,
    };
    r3.upper() = {
            relation::intermediate::join::key { j0, scalar::variable_reference { a1 } },
            relation::endpoint_kind::inclusive,
    };
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0], j0);
    EXPECT_EQ(r2.columns()[1], j1);

    ASSERT_EQ(r3.lower().keys().size(), 1);
    EXPECT_EQ(r3.lower().keys()[0].variable(), j0);
    EXPECT_EQ(r3.lower().keys()[0].value(), scalar::variable_reference(c0));

    ASSERT_EQ(r3.upper().keys().size(), 1);
    EXPECT_EQ(r3.upper().keys()[0].variable(), j0);
    EXPECT_EQ(r3.upper().keys()[0].value(), scalar::variable_reference(c1));

    EXPECT_EQ(r3.condition(), compare(c2, j1));

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, aggregate_group) {
    /*
     * values:r0 - project:r1 - aggregate_group:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto f0 = bindings(aggregate::declaration {
            aggregate::declaration::minimum_builtin_function_id + 1,
            "f0",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });
    auto f1 = bindings(aggregate::declaration {
            aggregate::declaration::minimum_builtin_function_id + 2,
            "f1",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r2 = p.insert(relation::intermediate::aggregate {
            {
                    a2,
            },
            {
                    {
                            f0, { a0 }, x0,
                    },
                    {
                            f1, { a1 }, x1,
                    },
            },
    });
    auto&& ro = p.insert(relation::emit {
            x0,
            x1,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    EXPECT_EQ(r2.group_keys().size(), 1);
    EXPECT_EQ(r2.group_keys()[0], c2);

    EXPECT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0].function(), f0);
    EXPECT_EQ(r2.columns()[0].arguments().size(), 1);
    EXPECT_EQ(r2.columns()[0].arguments()[0], c0);
    EXPECT_EQ(r2.columns()[0].destination(), x0);
    EXPECT_EQ(r2.columns()[1].function(), f1);
    EXPECT_EQ(r2.columns()[1].arguments().size(), 1);
    EXPECT_EQ(r2.columns()[1].arguments()[0], c1);
    EXPECT_EQ(r2.columns()[1].destination(), x1);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), x0);
    EXPECT_EQ(ro.columns()[1].source(), x1);
}

TEST_F(remove_variable_aliases_test, distinct_group) {
    /*
     * values:r0 - project:r1 - distinct_group:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto&& r2 = p.insert(relation::intermediate::distinct {
            a0, a1,
    });
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    EXPECT_EQ(r2.group_keys().size(), 2);
    EXPECT_EQ(r2.group_keys()[0], c0);
    EXPECT_EQ(r2.group_keys()[1], c1);

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, limit_group) {
    /*
     * values:r0 - project:r1 - limit_group:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto&& r2 = p.insert(relation::intermediate::limit {
            10,
            {
                    a0,
            },
            {
                    {
                            a1,
                    },
            },
    });
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    EXPECT_EQ(r2.group_keys().size(), 1);
    EXPECT_EQ(r2.group_keys()[0], c0);

    EXPECT_EQ(r2.sort_keys().size(), 1);
    EXPECT_EQ(r2.sort_keys()[0].variable(), c1);

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, union_group) {
    /*
     * values:r0 - project:r1 - union_group:r3 - emit:ro
     *                         /
     *            values:r2 --/
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto&& r2 = p.insert(relation::values {
            { j0, j1 },
            {
                    { constant(100), constant(200), },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto&& r3 = p.insert(relation::intermediate::union_ {
            { a0, j0, x0 },
            { a1, j1, x1 },
            { a2, {}, x2 },
    });
    auto&& ro = p.insert(relation::emit {
            x0,
            x1,
            x2,
    });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0], j0);
    EXPECT_EQ(r2.columns()[1], j1);

    ASSERT_EQ(r3.mappings().size(), 3);
    EXPECT_EQ(r3.mappings()[0].left(), c0);
    EXPECT_EQ(r3.mappings()[0].right(), j0);
    EXPECT_EQ(r3.mappings()[0].destination(), x0);
    EXPECT_EQ(r3.mappings()[1].left(), c1);
    EXPECT_EQ(r3.mappings()[1].right(), j1);
    EXPECT_EQ(r3.mappings()[1].destination(), x1);
    EXPECT_EQ(r3.mappings()[2].left(), c2);
    EXPECT_EQ(r3.mappings()[2].right(), std::nullopt);
    EXPECT_EQ(r3.mappings()[2].destination(), x2);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), x0);
    EXPECT_EQ(ro.columns()[1].source(), x1);
    EXPECT_EQ(ro.columns()[2].source(), x2);
}

TEST_F(remove_variable_aliases_test, intersection_group) {
    /*
     * values:r0 - project:r1 - intersection_group:r3 - emit:ro
     *                         /
     *            values:r2 --/
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto&& r2 = p.insert(relation::values {
            { j0, j1 },
            {
                    { constant(100), constant(200), },
            },
    });
    auto&& r3 = p.insert(relation::intermediate::intersection {
            { a0, j0 },
            { a1, j1 },
    });
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0], j0);
    EXPECT_EQ(r2.columns()[1], j1);

    ASSERT_EQ(r3.group_key_pairs().size(), 2);
    EXPECT_EQ(r3.group_key_pairs()[0].left(), c0);
    EXPECT_EQ(r3.group_key_pairs()[0].right(), j0);
    EXPECT_EQ(r3.group_key_pairs()[1].left(), c1);
    EXPECT_EQ(r3.group_key_pairs()[1].right(), j1);

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, difference_group) {
    /*
     * values:r0 - project:r1 - difference_group:r3 - emit:ro
     *                         /
     *            values:r2 --/
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto j0 = bindings.stream_variable("j0");
    auto j1 = bindings.stream_variable("j1");
    auto&& r2 = p.insert(relation::values {
            { j0, j1 },
            {
                    { constant(100), constant(200), },
            },
    });
    auto&& r3 = p.insert(relation::intermediate::difference {
            { a0, j0 },
            { a1, j1 },
    });
    auto&& ro = p.insert(relation::emit {
            a2,
    });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0], j0);
    EXPECT_EQ(r2.columns()[1], j1);

    ASSERT_EQ(r3.group_key_pairs().size(), 2);
    EXPECT_EQ(r3.group_key_pairs()[0].left(), c0);
    EXPECT_EQ(r3.group_key_pairs()[0].right(), j0);
    EXPECT_EQ(r3.group_key_pairs()[1].left(), c1);
    EXPECT_EQ(r3.group_key_pairs()[1].right(), j1);

    ASSERT_EQ(ro.columns().size(), 1);
    EXPECT_EQ(ro.columns()[0].source(), c2);
}

TEST_F(remove_variable_aliases_test, escape) {
    /*
     * values:r0 - project:r1 - escape:r2 - emit:ro
     */
    relation::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = p.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
                    { constant(3), constant(4), constant(5), },
            },
    });
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto a2 = bindings.stream_variable("a2");
    auto&& r1 = p.insert(relation::project {
            relation::project::column { a0, scalar::variable_reference { c0 } },
            relation::project::column { a1, scalar::variable_reference { c1 } },
            relation::project::column { a2, scalar::variable_reference { c2 } },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto&& r2 = p.insert(relation::intermediate::escape {
            { a0, x0 },
            { a1, x1 },
            { a2, x2 },
    });
    auto&& ro = p.insert(relation::emit {
            x0,
            x1,
            x2,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0], c0);
    EXPECT_EQ(r0.columns()[1], c1);
    EXPECT_EQ(r0.columns()[2], c2);

    ASSERT_EQ(r1.columns().size(), 0);

    ASSERT_EQ(r2.mappings().size(), 3);
    EXPECT_EQ(r2.mappings()[0].source(), c0);
    EXPECT_EQ(r2.mappings()[0].destination(), x0);
    EXPECT_EQ(r2.mappings()[1].source(), c1);
    EXPECT_EQ(r2.mappings()[1].destination(), x1);
    EXPECT_EQ(r2.mappings()[2].source(), c2);
    EXPECT_EQ(r2.mappings()[2].destination(), x2);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), x0);
    EXPECT_EQ(ro.columns()[1].source(), x1);
    EXPECT_EQ(ro.columns()[2].source(), x2);
}

TEST_F(remove_variable_aliases_test, subquery) {
    /* (inner query)
     * values:i0 -*
     */
    relation::graph_type inner;
    auto c0 = bindings.stream_variable("c0");
    auto&& i0 = inner.insert(relation::values {
            { c0 },
            {},
    });
    (void) i0;

    /*
     * subquery:r0 - emit:ro
     */
    relation::graph_type p;
    auto s0 = bindings.stream_variable("s0");
    auto&& r0 = p.insert(extension::relation::subquery {
            std::move(inner),
            {
                    { c0, s0 },
            },
    });
    auto&& ro = p.insert(relation::emit {
            s0,
    });
    r0.output() >> ro.input();

    EXPECT_THROW(apply(p), std::logic_error);
}

} // namespace yugawara::analyzer::details
