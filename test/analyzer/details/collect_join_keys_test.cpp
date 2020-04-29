#include <analyzer/details/collect_join_keys.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/join.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <analyzer/details/remove_redundant_conditions.h>

#include "utils.h"

namespace yugawara::analyzer::details {

using ::takatori::scalar::unary;
using ::takatori::scalar::binary;
using ::takatori::scalar::unary_operator;
using ::takatori::scalar::binary_operator;
using ::takatori::scalar::comparison_operator;

using cmp = ::takatori::scalar::comparison_operator;

using ::takatori::relation::endpoint_kind;

class collect_join_keys_test: public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    binding::factory bindings { creator };

    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table("T0", {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];
    storage::column const& t0c2 = t0->columns()[2];

    std::shared_ptr<storage::index> i0 = storages.add_index("I0", { t0, "I0", });

    std::shared_ptr<storage::table> t1 = storages.add_table("T1", {
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t1c0 = t1->columns()[0];
    storage::column const& t1c1 = t1->columns()[1];
    storage::column const& t1c2 = t1->columns()[2];

    std::shared_ptr<storage::index> i1 = storages.add_index("I1", { t1, "I1" });

    void apply(
            relation::graph_type& graph,
            collect_join_keys_feature_set features = collect_join_keys_feature_universe) {
        flow_volume_info vinfo { creator };
        collect_join_keys(graph, vinfo, features, creator);
        remove_redundant_conditions(graph);
    }
};

template<class T, class U = T>
static bool eq(::takatori::util::sequence_view<T const> actual, std::initializer_list<U const> expect) {
    return std::equal(actual.begin(), actual.end(), expect.begin(), expect.end());
}

TEST_F(collect_join_keys_test, stay) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(constant(1), constant(2), comparison_operator::less),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.left());
    EXPECT_GT(inr.output(), join.right());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::unbound);
    ASSERT_EQ(join.lower().keys().size(), 0);

    EXPECT_EQ(join.upper().kind(), endpoint_kind::unbound);
    ASSERT_EQ(join.upper().keys().size(), 0);

    EXPECT_EQ(join.condition(), compare(constant(1), constant(2), comparison_operator::less));
}

TEST_F(collect_join_keys_test, cogroup) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.left());
    EXPECT_GT(inr.output(), join.right());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    EXPECT_EQ(join.lower().keys()[0].variable(), cr0);
    EXPECT_EQ(join.lower().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    EXPECT_EQ(join.upper().keys()[0].variable(), cr0);
    EXPECT_EQ(join.upper().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.condition(), nullptr);
}

TEST_F(collect_join_keys_test, right_find) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c0), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(
                    varref { cr0 },
                    binary {
                            binary_operator::add,
                            varref { cl0 },
                            varref { cl1 },
                    }),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.left());
    EXPECT_GT(inr.output(), join.right());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    EXPECT_EQ(join.lower().keys()[0].variable(), cr0);
    EXPECT_EQ(join.lower().keys()[0].value(), (binary {
            binary_operator::add,
            varref { cl0 },
            varref { cl1 },
    }));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    EXPECT_EQ(join.upper().keys()[0].variable(), join.lower().keys()[0].variable());
    EXPECT_EQ(join.upper().keys()[0].value(), join.lower().keys()[0].value());

    EXPECT_EQ(join.condition(), nullptr);
}

TEST_F(collect_join_keys_test, right_scan) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c0), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            land(
                    compare(constant(0), varref(cr0), cmp::less_equal),
                    compare(varref(cr0), varref(cl0), cmp::less_equal)),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.left());
    EXPECT_GT(inr.output(), join.right());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    EXPECT_EQ(join.lower().keys()[0].variable(), cr0);
    EXPECT_EQ(join.lower().keys()[0].value(), constant(0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    EXPECT_EQ(join.upper().keys()[0].variable(), join.lower().keys()[0].variable());
    EXPECT_EQ(join.upper().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.condition(), nullptr);
}

TEST_F(collect_join_keys_test, left_find) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c0), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(
                    varref { cl0 },
                    binary {
                            binary_operator::add,
                            varref { cr0 },
                            varref { cr1 },
                    }),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.right());
    EXPECT_GT(inr.output(), join.left());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    EXPECT_EQ(join.lower().keys()[0].variable(), cl0);
    EXPECT_EQ(join.lower().keys()[0].value(), (binary {
            binary_operator::add,
            varref { cr0 },
            varref { cr1 },
    }));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    EXPECT_EQ(join.upper().keys()[0].variable(), join.lower().keys()[0].variable());
    EXPECT_EQ(join.upper().keys()[0].value(), join.lower().keys()[0].value());

    EXPECT_EQ(join.condition(), nullptr);
}

TEST_F(collect_join_keys_test, left_scan) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c0), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            land(
                    compare(constant(0), varref(cl0), cmp::less_equal),
                    compare(varref(cl0), varref(cr0), cmp::less_equal)),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.right());
    EXPECT_GT(inr.output(), join.left());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    EXPECT_EQ(join.lower().keys()[0].variable(), cl0);
    EXPECT_EQ(join.lower().keys()[0].value(), constant(0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    EXPECT_EQ(join.upper().keys()[0].variable(), join.lower().keys()[0].variable());
    EXPECT_EQ(join.upper().keys()[0].value(), varref(cr0));

    EXPECT_EQ(join.condition(), nullptr);
}

TEST_F(collect_join_keys_test, full_outer) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c0), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            land(
                    compare(cl0, cr0),
                    compare(cl1, cr1, cmp::less_equal)),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.left());
    EXPECT_GT(inr.output(), join.right());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    EXPECT_EQ(join.lower().keys()[0].variable(), cr0);
    EXPECT_EQ(join.lower().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    EXPECT_EQ(join.upper().keys()[0].variable(), join.lower().keys()[0].variable());
    EXPECT_EQ(join.upper().keys()[0].value(), join.lower().keys()[0].value());

    EXPECT_EQ(join.condition(), compare(cl1, cr1, cmp::less_equal));
}

TEST_F(collect_join_keys_test, left_outer) {
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl0");
    auto&& inl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), cl0 },
                    { bindings(t0c1), cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr0");
    auto&& inr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { bindings(t1c0), cr0 },
                    { bindings(t1c0), cr1 },
            },
    });
    auto&& join = r.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            land(
                    compare(cl0, cr0),
                    compare(constant(0), varref(cl1), cmp::less_equal)),
    });

    auto&& out = r.insert(relation::emit { cl0, cr0 });
    inl.output() >> join.left();
    inr.output() >> join.right();
    join.output() >> out.input();

    apply(r);
    EXPECT_GT(inl.output(), join.left());
    EXPECT_GT(inr.output(), join.right());

    EXPECT_EQ(join.lower().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.lower().keys().size(), 1);
    EXPECT_EQ(join.lower().keys()[0].variable(), cr0);
    EXPECT_EQ(join.lower().keys()[0].value(), varref(cl0));

    EXPECT_EQ(join.upper().kind(), endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(join.upper().keys().size(), 1);
    EXPECT_EQ(join.upper().keys()[0].variable(), join.lower().keys()[0].variable());
    EXPECT_EQ(join.upper().keys()[0].value(), join.lower().keys()[0].value());

    EXPECT_EQ(join.condition(), compare(constant(0), varref(cl1), cmp::less_equal));
}

} // namespace yugawara::analyzer::details
