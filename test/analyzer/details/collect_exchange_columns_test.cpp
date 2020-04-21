#include <analyzer/details/collect_exchange_columns.h>

#include <gtest/gtest.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>

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

#include "utils.h"

namespace yugawara::analyzer::details {

class collect_exchange_columns_test : public ::testing::Test {
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
            "testing",
            relation::set_quantifier::distinct,
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });

    details::exchange_column_info_map apply(plan::graph_type& graph) {
        details::collect_step_relations(graph);
        return details::collect_exchange_columns(graph, creator);
    }

    plan::exchange& create_source(
            std::initializer_list<descriptor::variable> variables,
            plan::exchange& exchange) {
        std::vector<
                relation::scan::column,
                ::takatori::util::object_allocator<relation::scan::column>> mappings;

        std::size_t index = 0;
        for (auto&& variable : variables) {
            mappings.emplace_back(bindings(t1->columns()[index++]), variable);
        }

        auto&& source = exchange.owner().insert(plan::process {});
        auto& r0 = source.operators().insert(relation::scan {
                bindings(*i1),
                std::move(mappings),
                {},
                {},
                {},
        });
        auto& r1 = source.operators().insert(offer {
                bindings(exchange),
        });
        r0.output() >> r1.input();
        return exchange;
    }

    plan::forward& create_sink(plan::graph_type& graph) {
        auto&& exchange = graph.insert(plan::forward {});
        auto&& sink = graph.insert(plan::process {});
        auto& r1 = sink.operators().insert(take_flat {
                bindings(exchange),
        });
        auto& r2 = sink.operators().insert(relation::emit {}); // no emission
        r1.output() >> r2.input();
        return exchange;
    }
};

inline bool is_exchange(descriptor::variable const& v) {
    return binding::unwrap(v).kind() == binding::variable_info_kind::exchange_column;
}

TEST_F(collect_exchange_columns_test, simple) {
    /*
     * [scan:r0 - emit:r1]:p0
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = p0.operators().insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();

    auto excs = apply(p);
    EXPECT_EQ(excs.size(), 0);

    // stream variables unchanged
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r1.columns()[0].source(), c0);
}

TEST_F(collect_exchange_columns_test, find) {
    /*
     * [find:r0 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = p0.operators().insert(relation::find {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
            {
                    relation::find::key { t0c0, constant(), }
            },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
}

TEST_F(collect_exchange_columns_test, scan) {
    /*
     * [scan:r0 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
}

TEST_F(collect_exchange_columns_test, join_find_index) {
    /*
     * [scan:r0 - join_find:r1]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = p0.operators().insert(relation::join_find {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
            {
                    relation::join_find::key { t0c0, constant(), }
            },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 4);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
    EXPECT_EQ(ro.columns()[3].source(), c3);
}

TEST_F(collect_exchange_columns_test, join_find_broadcast) {
    /*
     *        ...
     *           \
     * [scan:r0 - join_find:r1]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& broadcast = create_source({ c2, c3 }, p.insert(plan::broadcast {}));
    auto& r1 = p0.operators().insert(relation::join_find {
            relation::join_kind::inner,
            bindings(broadcast),
            {},
            {
                    relation::join_find::key { c2, constant(), }
            },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 4);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
    EXPECT_EQ(ro.columns()[3].source(), c3);
}

TEST_F(collect_exchange_columns_test, join_find_semi) {
    /*
     * [scan:r0 - join_find:r1]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = p0.operators().insert(relation::join_find {
            relation::join_kind::semi,
            bindings(*i0),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
            {
                    relation::join_find::key { t0c0, constant(), }
            },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
}

TEST_F(collect_exchange_columns_test, join_scan_index) {
    /*
     * [scan:r0 - join_scan:r1]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = p0.operators().insert(relation::join_scan {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
            {},
            {},
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 4);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
    EXPECT_EQ(ro.columns()[3].source(), c3);
}

TEST_F(collect_exchange_columns_test, join_scan_broadcast) {
    /*
     *        ...
     *           \
     * [scan:r0 - join_scan:r1]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& broadcast = create_source({ c2, c3 }, p.insert(plan::broadcast {}));
    auto& r1 = p0.operators().insert(relation::join_scan {
            relation::join_kind::inner,
            bindings(broadcast),
            {},
            {},
            {},
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 4);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
    EXPECT_EQ(ro.columns()[3].source(), c3);
}

TEST_F(collect_exchange_columns_test, join_scan_anti) {
    /*
     * [scan:r0 - join_scan:r1]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto c3 = bindings.stream_variable("c3");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = p0.operators().insert(relation::join_scan {
            relation::join_kind::anti,
            bindings(*i0),
            {
                    { t1c0, c2 },
                    { t1c1, c3 },
            },
            {},
            {},
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
}

TEST_F(collect_exchange_columns_test, project) {
    /*
     * [scan:r0 - project:r1 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto c2 = bindings.stream_variable("c3");
    auto c3 = bindings.stream_variable("c3");
    auto& r1 = p0.operators().insert(relation::project {
            relation::project::column {
                    constant(), c2,
            },
            relation::project::column {
                    constant(), c3,
            },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 4);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
    EXPECT_EQ(ro.columns()[3].source(), c3);
}

TEST_F(collect_exchange_columns_test, filter) {
    /*
     * [scan:r0 - filter:r1 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = p0.operators().insert(relation::filter {
            constant(),
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
}

TEST_F(collect_exchange_columns_test, buffer) {
    /*
     * [scan:r0 - buffer:r1 - project:r2 - offer:ro0]:p0 - ...
     *                  \
     *                   ---- project:r3 - offer:ro1] - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& r1 = p0.operators().insert(relation::buffer { 2 });
    auto c2 = bindings.stream_variable("c2");
    auto& r2 = p0.operators().insert(relation::project {
            relation::project::column {
                    constant(), c2,
            },
    });
    auto& ro0 = p0.operators().insert(offer {
            bindings(sink),
    });
    auto c3 = bindings.stream_variable("c3");
    auto& r3 = p0.operators().insert(relation::project {
            relation::project::column {
                    constant(), c3,
            },
    });
    auto& ro1 = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output_ports()[0] >> r2.input();
    r2.output() >> ro0.input();
    r1.output_ports()[1] >> r3.input();
    r3.output() >> ro1.input();

    apply(p);

    ASSERT_EQ(ro0.columns().size(), 3);
    EXPECT_EQ(ro0.columns()[0].source(), c0);
    EXPECT_EQ(ro0.columns()[1].source(), c1);
    EXPECT_EQ(ro0.columns()[2].source(), c2);

    ASSERT_EQ(ro1.columns().size(), 3);
    EXPECT_EQ(ro1.columns()[0].source(), c0);
    EXPECT_EQ(ro1.columns()[1].source(), c1);
    EXPECT_EQ(ro1.columns()[2].source(), c3);
}

TEST_F(collect_exchange_columns_test, emit) {
    /*
     * [scan:r0 - emit:r1]:p0
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
            c0,
            c1,
    });
    r0.output() >> r1.input();

    apply(p);
    // ok.
}

TEST_F(collect_exchange_columns_test, write) {
    /*
     * [scan:r0 - emit:r1]:p0
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
                    { c1, t1c1, },
            },
    });
    r0.output() >> r1.input();

    apply(p);
    // ok.
}

TEST_F(collect_exchange_columns_test, escape) {
    /*
     * [scan:r0 - escape:r1 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto c2 = bindings.stream_variable("c3");
    auto c3 = bindings.stream_variable("c3");
    auto& r1 = p0.operators().insert(relation::intermediate::escape {
            { c0, c2 },
            { c1, c3 },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), c2);
    EXPECT_EQ(ro.columns()[1].source(), c3);
}

TEST_F(collect_exchange_columns_test, join_group) {
    /*
     * e0 -\
     *     [take_cogroup:r0 - join_group:r1 - offer:ro]:p0 - ...
     * e1 -/
     */
    plan::graph_type p;
    auto e0c0 = bindings.stream_variable("e0c0");
    auto e0c1 = bindings.stream_variable("e0c1");
    auto&& e0 = create_source({ e0c0, e0c1 }, p.insert(plan::group {
            {},
            { e0c0, },
    }));
    auto e1c0 = bindings.stream_variable("e1c0");
    auto e1c1 = bindings.stream_variable("e1c1");
    auto&& e1 = create_source({ e1c0, e1c1 }, p.insert(plan::group {
            {},
            { e1c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_cogroup {
            { bindings(e0) },
            { bindings(e1) },
    });
    auto& r1 = p0.operators().insert(relation::step::join {
            relation::join_kind::inner,
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 4);
    EXPECT_EQ(ro.columns()[0].source(), e0c0);
    EXPECT_EQ(ro.columns()[1].source(), e0c1);
    EXPECT_EQ(ro.columns()[2].source(), e1c0);
    EXPECT_EQ(ro.columns()[3].source(), e1c1);
}

TEST_F(collect_exchange_columns_test, join_group_semi) {
    /*
     * e0 -\
     *     [take_cogroup:r0 - join_group:r1 - offer:ro]:p0 - ...
     * e1 -/
     */
    plan::graph_type p;
    auto e0c0 = bindings.stream_variable("e0c0");
    auto e0c1 = bindings.stream_variable("e0c1");
    auto&& e0 = create_source({ e0c0, e0c1 }, p.insert(plan::group {
            {},
            { e0c0, },
    }));
    auto e1c0 = bindings.stream_variable("e1c0");
    auto e1c1 = bindings.stream_variable("e1c1");
    auto&& e1 = create_source({ e1c0, e1c1 }, p.insert(plan::group {
            {},
            { e1c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_cogroup {
            { bindings(e0) },
            { bindings(e1) },
    });
    auto& r1 = p0.operators().insert(relation::step::join {
            relation::join_kind::semi,
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), e0c0);
    EXPECT_EQ(ro.columns()[1].source(), e0c1);
}

TEST_F(collect_exchange_columns_test, aggregate_group) {
    /*
     * ... - [take_group:r0 - aggregate:r1 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& e0 = create_source({ c0, c1, c2, }, p.insert(plan::group {
            {},
            { c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_group {
            bindings(e0),
    });
    auto c3 = bindings.stream_variable("c3");
    auto& r1 = p0.operators().insert(relation::step::aggregate {
            {
                    bindings(agg0),
                    { c1 },
                    c3,
            },
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c3);
}

TEST_F(collect_exchange_columns_test, intersection_group) {
    /*
     * e0 -\
     *     [take_cogroup:r0 - intersection:r1 - offer:ro]:p0 - ...
     * e1 -/
     */
    plan::graph_type p;
    auto e0c0 = bindings.stream_variable("e0c0");
    auto e0c1 = bindings.stream_variable("e0c1");
    auto&& e0 = create_source({ e0c0, e0c1 }, p.insert(plan::group {
            {},
            { e0c0, },
    }));
    auto e1c0 = bindings.stream_variable("e1c0");
    auto e1c1 = bindings.stream_variable("e1c1");
    auto&& e1 = create_source({ e1c0, e1c1 }, p.insert(plan::group {
            {},
            { e1c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_cogroup {
            { bindings(e0) },
            { bindings(e1) },
    });
    auto& r1 = p0.operators().insert(relation::step::intersection {});
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), e0c0);
    EXPECT_EQ(ro.columns()[1].source(), e0c1);
}

TEST_F(collect_exchange_columns_test, difference_group) {
    /*
     * e0 -\
     *     [take_cogroup:r0 - difference:r1 - offer:ro]:p0 - ...
     * e1 -/
     */
    plan::graph_type p;
    auto e0c0 = bindings.stream_variable("e0c0");
    auto e0c1 = bindings.stream_variable("e0c1");
    auto&& e0 = create_source({ e0c0, e0c1 }, p.insert(plan::group {
            {},
            { e0c0, },
    }));
    auto e1c0 = bindings.stream_variable("e1c0");
    auto e1c1 = bindings.stream_variable("e1c1");
    auto&& e1 = create_source({ e1c0, e1c1 }, p.insert(plan::group {
            {},
            { e1c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_cogroup {
            { bindings(e0) },
            { bindings(e1) },
    });
    auto& r1 = p0.operators().insert(relation::step::difference {});
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 2);
    EXPECT_EQ(ro.columns()[0].source(), e0c0);
    EXPECT_EQ(ro.columns()[1].source(), e0c1);
}

TEST_F(collect_exchange_columns_test, flatten_group) {
    /*
     * ... - [take_group:r0 - flatten:r1 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& e0 = create_source({ c0, c1, c2, }, p.insert(plan::group {
            {},
            { c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_group {
            bindings(e0),
    });
    auto& r1 = p0.operators().insert(relation::step::flatten {});
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
}

TEST_F(collect_exchange_columns_test, take_flat) {
    /*
     * ... - [take_flat:r0 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& e0 = create_source({ c0, c1, c2, }, p.insert(plan::forward {}));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_flat {
            bindings(e0),
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0].source(), e0.output_columns()[0]);
    EXPECT_EQ(r0.columns()[1].source(), e0.output_columns()[1]);
    EXPECT_EQ(r0.columns()[2].source(), e0.output_columns()[2]);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c1);
    EXPECT_EQ(r0.columns()[2].destination(), c2);

    ASSERT_EQ(ro.columns().size(), 3);
    EXPECT_EQ(ro.columns()[0].source(), c0);
    EXPECT_EQ(ro.columns()[1].source(), c1);
    EXPECT_EQ(ro.columns()[2].source(), c2);
}


TEST_F(collect_exchange_columns_test, take_group) {
    /*
     * ... - [take_group:r0 - flatten:r1 - offer:ro]:p0 - ...
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& e0 = create_source({ c0, c1, c2, }, p.insert(plan::group {
            {},
            { c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_group {
            bindings(e0),
    });
    auto& r1 = p0.operators().insert(relation::step::flatten {});
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.columns().size(), 3);
    EXPECT_EQ(r0.columns()[0].source(), e0.output_columns()[0]);
    EXPECT_EQ(r0.columns()[1].source(), e0.output_columns()[1]);
    EXPECT_EQ(r0.columns()[2].source(), e0.output_columns()[2]);
    EXPECT_EQ(r0.columns()[0].destination(), c0);
    EXPECT_EQ(r0.columns()[1].destination(), c1);
    EXPECT_EQ(r0.columns()[2].destination(), c2);
}

TEST_F(collect_exchange_columns_test, take_cogroup) {
    /*
     * e0 -\
     *     [take_cogroup:r0 - join_group:r1 - offer:ro]:p0 - ...
     * e1 -/
     */
    plan::graph_type p;
    auto e0c0 = bindings.stream_variable("e0c0");
    auto e0c1 = bindings.stream_variable("e0c1");
    auto&& e0 = create_source({ e0c0, e0c1 }, p.insert(plan::group {
            {},
            { e0c0, },
    }));
    auto e1c0 = bindings.stream_variable("e1c0");
    auto e1c1 = bindings.stream_variable("e1c1");
    auto&& e1 = create_source({ e1c0, e1c1 }, p.insert(plan::group {
            {},
            { e1c0, },
    }));
    auto&& p0 = p.insert(plan::process {});
    auto&& sink = create_sink(p);

    auto& r0 = p0.operators().insert(take_cogroup {
            { bindings(e0) },
            { bindings(e1) },
    });
    auto& r1 = p0.operators().insert(relation::step::join {
            relation::join_kind::inner,
    });
    auto& ro = p0.operators().insert(offer {
            bindings(sink),
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(p);

    ASSERT_EQ(r0.groups().size(), 2);

    auto&& g0 = r0.groups()[0];
    ASSERT_EQ(g0.columns().size(), 2);
    EXPECT_EQ(g0.columns()[0].source(), e0.output_columns()[0]);
    EXPECT_EQ(g0.columns()[1].source(), e0.output_columns()[1]);
    EXPECT_EQ(g0.columns()[0].destination(), e0c0);
    EXPECT_EQ(g0.columns()[1].destination(), e0c1);

    auto&& g1 = r0.groups()[1];
    ASSERT_EQ(g1.columns().size(), 2);
    EXPECT_EQ(g1.columns()[0].source(), e1.output_columns()[0]);
    EXPECT_EQ(g1.columns()[1].source(), e1.output_columns()[1]);
    EXPECT_EQ(g1.columns()[0].destination(), e1c0);
    EXPECT_EQ(g1.columns()[1].destination(), e1c1);
}

TEST_F(collect_exchange_columns_test, offer) {
    /*
     * [scan:r0 - offer:r1]:p0 - ..
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& e0 = create_sink(p);

    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    r0.output() >> r1.input();

    apply(p);

    ASSERT_EQ(r1.columns().size(), 3);
    EXPECT_EQ(r1.columns()[0].source(), c0);
    EXPECT_EQ(r1.columns()[1].source(), c1);
    EXPECT_EQ(r1.columns()[2].source(), c2);
    EXPECT_EQ(r1.columns()[0].destination(), e0.columns()[0]);
    EXPECT_EQ(r1.columns()[1].destination(), e0.columns()[1]);
    EXPECT_EQ(r1.columns()[2].destination(), e0.columns()[2]);
}

TEST_F(collect_exchange_columns_test, union) {
    /*
     * [scan:r0 - offer:r1]:p0
     *                    \
     *                    [forward]:e0 - [take_flat:r4 - emit:r5]:p1
     *                    /
     * [scan:r2 - offer:r3]:p1
     */
    plan::graph_type p;
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    auto&& p2 = p.insert(plan::process {});

    auto e0c0 = bindings.stream_variable("e0c0");
    auto e0c1 = bindings.stream_variable("e0c1");
    auto e0c2 = bindings.stream_variable("e0c2");
    auto&& e0 = p.insert(plan::forward {
            { e0c0, e0c1, e0c2, },
    });

    auto p0c0 = bindings.stream_variable("p0c0");
    auto p0c1 = bindings.stream_variable("p0c1");
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, p0c0 },
                    { t0c1, p0c1 },
            },
    });
    auto& r1 = p0.operators().insert(offer {
            bindings(e0),
            {
                    { p0c0, e0c0 },
                    { p0c1, e0c1 },
            },
    });
    r0.output() >> r1.input();

    auto p1c0 = bindings.stream_variable("p1c0");
    auto p1c1 = bindings.stream_variable("p1c1");
    auto& r2 = p1.operators().insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, p1c0 },
                    { t1c1, p1c1 },
            },
    });
    auto& r3 = p1.operators().insert(offer {
            bindings(e0),
            {
                    { p1c0, e0c0 },
                    { p1c1, e0c2 },
            },
    });
    r2.output() >> r3.input();

    auto& r4 = p2.operators().insert(take_flat {
            bindings(e0),
    });
    auto& r5 = p2.operators().insert(relation::emit {
            e0c0,
            e0c1,
            e0c2,
    });
    r4.output() >> r5.input();

    auto excs = apply(p);
    EXPECT_EQ(excs.size(), 1);

    ASSERT_EQ(e0.columns().size(), 3);
    auto&& x0 = e0.columns()[0];
    auto&& x1 = e0.columns()[1];
    auto&& x2 = e0.columns()[2];

    EXPECT_TRUE(is_exchange(x0));
    EXPECT_TRUE(is_exchange(x1));
    EXPECT_TRUE(is_exchange(x2));

    // left offer
    ASSERT_EQ(r1.columns().size(), 2);
    EXPECT_EQ(r1.columns()[0].source(), p0c0);
    EXPECT_EQ(r1.columns()[1].source(), p0c1);
    EXPECT_EQ(r1.columns()[0].destination(), x0);
    EXPECT_EQ(r1.columns()[1].destination(), x1);

    // right offer
    ASSERT_EQ(r3.columns().size(), 2);
    EXPECT_EQ(r3.columns()[0].source(), p1c0);
    EXPECT_EQ(r3.columns()[1].source(), p1c1);
    EXPECT_EQ(r3.columns()[0].destination(), x0);
    EXPECT_EQ(r3.columns()[1].destination(), x2);

    // union take
    ASSERT_EQ(r4.columns().size(), 3);
    EXPECT_EQ(r4.columns()[0].source(), x0);
    EXPECT_EQ(r4.columns()[1].source(), x1);
    EXPECT_EQ(r4.columns()[2].source(), x2);
    EXPECT_EQ(r4.columns()[0].destination(), e0c0);
    EXPECT_EQ(r4.columns()[1].destination(), e0c1);
    EXPECT_EQ(r4.columns()[2].destination(), e0c2);
}

TEST_F(collect_exchange_columns_test, forward) {
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
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    auto& r2 = p1.operators().insert(take_flat {
            bindings(e0),
    });
    auto& r3 = p1.operators().insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.input();

    auto excs = apply(p);
    EXPECT_EQ(excs.size(), 1);

    ASSERT_EQ(e0.columns().size(), 3);
    auto&& e0c0 = e0.columns()[0];
    auto&& e0c1 = e0.columns()[1];
    auto&& e0c2 = e0.columns()[2];

    EXPECT_TRUE(is_exchange(e0c0));
    EXPECT_TRUE(is_exchange(e0c1));
    EXPECT_TRUE(is_exchange(e0c2));

    ASSERT_EQ(r1.columns().size(), 3);
    EXPECT_EQ(r1.columns()[0].source(), c0);
    EXPECT_EQ(r1.columns()[1].source(), c1);
    EXPECT_EQ(r1.columns()[2].source(), c2);
    EXPECT_EQ(r1.columns()[0].destination(), e0c0);
    EXPECT_EQ(r1.columns()[1].destination(), e0c1);
    EXPECT_EQ(r1.columns()[2].destination(), e0c2);

    ASSERT_EQ(r2.columns().size(), 3);
    EXPECT_EQ(r2.columns()[0].source(), e0c0);
    EXPECT_EQ(r2.columns()[1].source(), e0c1);
    EXPECT_EQ(r2.columns()[2].source(), e0c2);
    EXPECT_EQ(r2.columns()[0].destination(), c0);
    EXPECT_EQ(r2.columns()[1].destination(), c1);
    EXPECT_EQ(r2.columns()[2].destination(), c2);
}

TEST_F(collect_exchange_columns_test, group) {
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

    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    auto& r2 = p1.operators().insert(take_group {
            bindings(e0),
    });
    auto& r3 = p1.operators().insert(flatten {});
    auto& r4 = p1.operators().insert(relation::emit {
            c0,
            c1,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    auto excs = apply(p);
    EXPECT_EQ(excs.size(), 1);

    ASSERT_EQ(e0.columns().size(), 3);
    auto&& e0c0 = e0.columns()[0];
    auto&& e0c1 = e0.columns()[1];
    auto&& e0c2 = e0.columns()[2];

    EXPECT_TRUE(is_exchange(e0c0));
    EXPECT_TRUE(is_exchange(e0c1));
    EXPECT_TRUE(is_exchange(e0c2));

    ASSERT_EQ(e0.group_keys().size(), 1);
    ASSERT_EQ(e0.group_keys()[0], e0c0);

    ASSERT_EQ(r1.columns().size(), 3);
    EXPECT_EQ(r1.columns()[0].source(), c0);
    EXPECT_EQ(r1.columns()[1].source(), c1);
    EXPECT_EQ(r1.columns()[2].source(), c2);
    EXPECT_EQ(r1.columns()[0].destination(), e0c0);
    EXPECT_EQ(r1.columns()[1].destination(), e0c1);
    EXPECT_EQ(r1.columns()[2].destination(), e0c2);

    ASSERT_EQ(r2.columns().size(), 3);
    EXPECT_EQ(r2.columns()[0].source(), e0c0);
    EXPECT_EQ(r2.columns()[1].source(), e0c1);
    EXPECT_EQ(r2.columns()[2].source(), e0c2);
    EXPECT_EQ(r2.columns()[0].destination(), c0);
    EXPECT_EQ(r2.columns()[1].destination(), c1);
    EXPECT_EQ(r2.columns()[2].destination(), c2);
}

TEST_F(collect_exchange_columns_test, aggregate) {
    /*
     * [scan:r0 - offer:r1]:p0 - [aggregate]:e0 - [take_group:r2 - flatten:r3 - emit:r4]:p1
     */
    plan::graph_type p;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto a0 = bindings.stream_variable("a0");
    auto&& p0 = p.insert(plan::process {});
    auto&& p1 = p.insert(plan::process {});
    // group be c0, aggregate f(c1) as a0
    auto&& e0 = p.insert(plan::aggregate {
            {},
            {},
            { c0, },
            {
                    {
                            bindings(agg0),
                            { c1, },
                            a0,
                    }
            },
    });

    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = p0.operators().insert(offer {
            bindings(e0),
    });
    auto& r2 = p1.operators().insert(take_group {
            bindings(e0),
    });
    auto& r3 = p1.operators().insert(flatten {});
    auto& r4 = p1.operators().insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    auto excs = apply(p);
    EXPECT_EQ(excs.size(), 1);

    ASSERT_EQ(e0.source_columns().size(), 3);
    auto&& e0s0 = e0.source_columns()[0];
    auto&& e0s1 = e0.source_columns()[1];
    auto&& e0s2 = e0.source_columns()[2];

    ASSERT_EQ(e0.destination_columns().size(), 2);
    auto&& e0d0 = e0.destination_columns()[0];
    auto&& e0d1 = e0.destination_columns()[1];

    EXPECT_TRUE(is_exchange(e0s0));
    EXPECT_TRUE(is_exchange(e0s1));
    EXPECT_TRUE(is_exchange(e0s2));
    EXPECT_TRUE(is_exchange(e0d0));
    EXPECT_TRUE(is_exchange(e0d1));

    EXPECT_EQ(e0d0, e0s0);

    ASSERT_EQ(e0.group_keys().size(), 1);
    ASSERT_EQ(e0.group_keys()[0], e0s0);

    ASSERT_EQ(e0.aggregations().size(), 1);
    {
        auto&& a = e0.aggregations()[0];
        ASSERT_EQ(a.arguments().size(), 1);
        EXPECT_EQ(a.arguments()[0], e0s1);
        EXPECT_EQ(a.destination(), e0d1);
    }

    ASSERT_EQ(r1.columns().size(), 3);
    EXPECT_EQ(r1.columns()[0].source(), c0);
    EXPECT_EQ(r1.columns()[1].source(), c1);
    EXPECT_EQ(r1.columns()[2].source(), c2);
    EXPECT_EQ(r1.columns()[0].destination(), e0s0);
    EXPECT_EQ(r1.columns()[1].destination(), e0s1);
    EXPECT_EQ(r1.columns()[2].destination(), e0s2);

    ASSERT_EQ(r2.columns().size(), 2);
    EXPECT_EQ(r2.columns()[0].source(), e0d0);
    EXPECT_EQ(r2.columns()[1].source(), e0d1);
    EXPECT_EQ(r2.columns()[0].destination(), c0);
    EXPECT_EQ(r2.columns()[1].destination(), a0);
}

TEST_F(collect_exchange_columns_test, broadcast) {
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
    auto& r0 = p0.operators().insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, p0c0 },
                    { t0c1, p0c1 },
                    { t0c2, p0c2 },
            },
    });
    auto& r1 = p0.operators().insert(offer {
            bindings(e0),
    });

    auto p1c0 = bindings.stream_variable("p1c0");
    auto p1c1 = bindings.stream_variable("p1c1");
    auto p1c2 = bindings.stream_variable("p1c2");
    auto& r2 = p1.operators().insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, p1c0 },
                    { t1c1, p1c1 },
                    { t1c2, p1c2 },
            },
    });

    auto& r3 = p1.operators().insert(relation::join_find {
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

    auto& r4 = p1.operators().insert(relation::emit {
            p0c1,
            p1c1,
    });
    r0.output() >> r1.input();
    r2.output() >> r3.left();
    r3.output() >> r4.input();

    auto excs = apply(p);
    EXPECT_EQ(excs.size(), 1);

    ASSERT_EQ(e0.columns().size(), 3);
    auto&& e0c0 = e0.columns()[0];
    auto&& e0c1 = e0.columns()[1];
    auto&& e0c2 = e0.columns()[2];

    EXPECT_TRUE(is_exchange(e0c0));
    EXPECT_TRUE(is_exchange(e0c1));
    EXPECT_TRUE(is_exchange(e0c2));

    ASSERT_EQ(r1.columns().size(), 3);
    EXPECT_EQ(r1.columns()[0].source(), p0c0);
    EXPECT_EQ(r1.columns()[1].source(), p0c1);
    EXPECT_EQ(r1.columns()[2].source(), p0c2);
    EXPECT_EQ(r1.columns()[0].destination(), e0c0);
    EXPECT_EQ(r1.columns()[1].destination(), e0c1);
    EXPECT_EQ(r1.columns()[2].destination(), e0c2);

    ASSERT_EQ(r3.columns().size(), 3);
    EXPECT_EQ(r3.columns()[0].source(), e0c0);
    EXPECT_EQ(r3.columns()[1].source(), e0c1);
    EXPECT_EQ(r3.columns()[2].source(), e0c2);
    EXPECT_EQ(r3.columns()[0].destination(), p0c0);
    EXPECT_EQ(r3.columns()[1].destination(), p0c1);
    EXPECT_EQ(r3.columns()[2].destination(), p0c2);

    ASSERT_EQ(r3.keys().size(), 1);
    EXPECT_EQ(r3.keys()[0].variable(), e0c0);
    EXPECT_EQ(r3.keys()[0].value(), scalar::variable_reference(p1c0));
}

} // namespace yugawara::analyzer::details
