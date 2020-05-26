#include <yugawara/analyzer/details/stream_variable_flow_info.h>

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

class stream_variable_flow_info_test : public ::testing::Test {
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

    relation::expression::input_port_type& connect(relation::expression::output_port_type& output) {
        if (output.opposite()) {
            throw std::domain_error("must not be connected");
        }

        auto&& r = output.owner().owner();

        /*
         *   .. -\
         *        join_relation - emit
         * scan -/
         */
        auto c0 = bindings.stream_variable("dummy0");
        auto&& right = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t1c0, c0 },
                },
        });
        auto&& join = r.insert(relation::intermediate::join {
                relation::join_kind::inner,
        });
        auto&& emit = r.insert(relation::emit {
                c0,
        });
        output >> join.left();
        right.output() >> join.right();
        join.output() >> emit.input();
        return join.left();
    }
};

TEST_F(stream_variable_flow_info_test, find) {
    /*
     * scan:r0 - ..
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

    auto&& input = connect(r0.output());

    stream_variable_flow_info info;
    auto&& entry = info.find(c0, input);
    ASSERT_TRUE(entry);

    EXPECT_EQ(entry->originator(), r0);
    EXPECT_TRUE(entry->find(c1));
    EXPECT_TRUE(entry->find(c2));
}

TEST_F(stream_variable_flow_info_test, scan) {
    /*
     * scan:r0 - ...
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

    auto&& input = connect(r0.output());

    stream_variable_flow_info info;
    auto&& entry = info.find(c0, input);
    ASSERT_TRUE(entry);

    EXPECT_EQ(entry->originator(), r0);
    EXPECT_TRUE(entry->find(c1));
    EXPECT_TRUE(entry->find(c2));
}

TEST_F(stream_variable_flow_info_test, join_relation) {
    /*
     * scan:rl -\
     *           join_relation:r0 - ...
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
    rl.output() >> r0.left();
    rr.output() >> r0.right();

    auto&& input = connect(r0.output());

    stream_variable_flow_info info;

    auto&& el = info.find(cl0, input);
    auto&& er = info.find(cr0, input);
    ASSERT_TRUE(el);
    ASSERT_TRUE(er);
    ASSERT_NE(el.get(), er.get());

    EXPECT_EQ(el->originator(), rl);
    EXPECT_TRUE(el->find(cl1));
    EXPECT_TRUE(el->find(cl2));

    EXPECT_EQ(er->originator(), rr);
    EXPECT_TRUE(er->find(cr1));
    EXPECT_TRUE(er->find(cr2));
}

TEST_F(stream_variable_flow_info_test, join_find) {
    /*
     * scan:r0 - join_find:r1 - ...
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
    r0.output() >> r1.left();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;

    auto&& el = info.find(c0, input);
    auto&& er = info.find(j0, input);
    ASSERT_TRUE(el);
    ASSERT_TRUE(er);
    ASSERT_NE(el.get(), er.get());
    ASSERT_EQ(&info.entry(r1.left()), el.get());

    EXPECT_EQ(el->originator(), r0);
    EXPECT_TRUE(el->find(c1));
    EXPECT_TRUE(el->find(c2));

    EXPECT_EQ(er->originator(), r1);
    EXPECT_TRUE(er->find(j1));
    EXPECT_TRUE(er->find(j2));
}

TEST_F(stream_variable_flow_info_test, join_scan) {
    /*
     * scan:r0 - join_scan:r1 - ...
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
    r0.output() >> r1.left();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;

    auto&& el = info.find(c0, input);
    auto&& er = info.find(j0, input);
    ASSERT_TRUE(el);
    ASSERT_TRUE(er);
    ASSERT_NE(el.get(), er.get());
    ASSERT_EQ(&info.entry(r1.left()), el.get());

    EXPECT_EQ(el->originator(), r0);
    EXPECT_TRUE(el->find(c1));
    EXPECT_TRUE(el->find(c2));

    EXPECT_EQ(er->originator(), r1);
    EXPECT_TRUE(er->find(j1));
    EXPECT_TRUE(er->find(j2));
}

TEST_F(stream_variable_flow_info_test, project) {
    /*
     * scan:r0 - project:r1 - ...
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
    r0.output() >> r1.input();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;
    auto&& entry = info.find(x0, input);
    ASSERT_TRUE(entry);

    EXPECT_EQ(entry->originator(), r0);
    EXPECT_EQ(entry->find(c1), r0);
    EXPECT_EQ(entry->find(c2), r0);
    EXPECT_EQ(entry->find(x0), r1);
    EXPECT_EQ(entry->find(x1), r1);
}

TEST_F(stream_variable_flow_info_test, filter) {
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
    r0.output() >> r1.input();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;
    auto&& entry = info.find(c0, input);
    ASSERT_TRUE(entry);

    EXPECT_EQ(entry->originator(), r0);
    EXPECT_EQ(entry->find(c1), r0);
    EXPECT_EQ(entry->find(c2), r0);
}

TEST_F(stream_variable_flow_info_test, buffer) {
    /*
     *                    /- ...
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
    r0.output() >> r1.input();

    auto&& i0 = connect(r1.output_ports()[0]);
    auto&& i1 = connect(r1.output_ports()[1]);

    stream_variable_flow_info info;
    auto&& e0 = info.find(c0, i0);
    auto&& e1 = info.find(c0, i1);
    ASSERT_TRUE(e0);
    ASSERT_TRUE(e1);
    EXPECT_NE(e0.get(), e1.get());

    EXPECT_EQ(e0->originator(), r0);
    EXPECT_EQ(e0->find(c1), r0);
    EXPECT_EQ(e0->find(c2), r0);

    EXPECT_EQ(e1->originator(), r0);
    EXPECT_EQ(e1->find(c1), r0);
    EXPECT_EQ(e1->find(c2), r0);
}

TEST_F(stream_variable_flow_info_test, aggregate_relation) {
    /*
     * scan:r0 - aggregate_relation:r1 - ...
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
    r0.output() >> r1.input();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;
    auto&& entry = info.find(x1, input);
    ASSERT_TRUE(entry);

    EXPECT_EQ(entry->originator(), r0);
    EXPECT_EQ(entry->find(c1), r0);
    EXPECT_EQ(entry->find(c2), r0);
    EXPECT_EQ(entry->find(x1), r1);
    EXPECT_EQ(entry->find(x2), r1);
}

TEST_F(stream_variable_flow_info_test, distinct_relation) {
    /*
     * scan:r0 - distinct_relation:r1 - ...
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
    r0.output() >> r1.input();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;
    auto&& entry = info.find(c0, input);
    ASSERT_TRUE(entry);

    EXPECT_EQ(entry->originator(), r0);
    EXPECT_EQ(entry->find(c1), r0);
    EXPECT_EQ(entry->find(c2), r0);
}

TEST_F(stream_variable_flow_info_test, limit_relation) {
    /*
     * scan:r0 - limit_relation:r1 - ...
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
    r0.output() >> r1.input();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;
    auto&& entry = info.find(c0, input);
    ASSERT_TRUE(entry);

    EXPECT_EQ(entry->originator(), r0);
    EXPECT_EQ(entry->find(c1), r0);
    EXPECT_EQ(entry->find(c2), r0);
}

TEST_F(stream_variable_flow_info_test, intersection_relation) {
    /*
     * scan:rl -\
     *           intersection_relation:r0 - ...
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
    rl.output() >> r0.left();
    rr.output() >> r0.right();

    auto&& input = connect(r0.output());

    stream_variable_flow_info info;

    auto&& el = info.find(cl0, input);
    auto&& er = info.find(cr0, input);
    ASSERT_TRUE(el);
    ASSERT_TRUE(er);
    ASSERT_NE(el.get(), er.get());

    EXPECT_EQ(el->originator(), rl);
    EXPECT_TRUE(el->find(cl1));
    EXPECT_TRUE(el->find(cl2));

    EXPECT_EQ(er->originator(), rr);
    EXPECT_TRUE(er->find(cr1));
    EXPECT_TRUE(er->find(cr2));
}

TEST_F(stream_variable_flow_info_test, difference_relation) {
    /*
     * scan:rl -\
     *           difference_relation:r0 - ...
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
    rl.output() >> r0.left();
    rr.output() >> r0.right();

    auto&& input = connect(r0.output());

    stream_variable_flow_info info;

    auto&& el = info.find(cl0, input);
    auto&& er = info.find(cr0, input);
    ASSERT_TRUE(el);
    ASSERT_TRUE(er);
    ASSERT_NE(el.get(), er.get());

    EXPECT_EQ(el->originator(), rl);
    EXPECT_TRUE(el->find(cl1));
    EXPECT_TRUE(el->find(cl2));

    EXPECT_EQ(er->originator(), rr);
    EXPECT_TRUE(er->find(cr1));
    EXPECT_TRUE(er->find(cr2));
}

TEST_F(stream_variable_flow_info_test, escape) {
    /*
     * scan:r0 - escape:r1 - ...
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
    r0.output() >> r1.input();

    auto&& input = connect(r1.output());

    stream_variable_flow_info info;
    EXPECT_FALSE(info.find(c0, input));
    EXPECT_FALSE(info.find(c1, input));
    EXPECT_FALSE(info.find(c2, input));

    auto&& entry = info.find(x0, input);
    ASSERT_TRUE(entry);
    EXPECT_EQ(entry->originator(), r1);
    EXPECT_EQ(entry->find(x1), r1);
    EXPECT_EQ(entry->find(x2), r1);
}

} // namespace yugawara::analyzer::details
