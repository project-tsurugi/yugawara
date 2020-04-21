#include <analyzer/details/rewrite_scan.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/find.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <analyzer/details/default_index_estimator.h>

#include "utils.h"

namespace yugawara::analyzer::details {

using ::takatori::scalar::unary;
using ::takatori::scalar::binary;
using ::takatori::scalar::unary_operator;
using ::takatori::scalar::binary_operator;
using ::takatori::scalar::comparison_operator;

using ::takatori::util::object_ownership_reference;
using ::takatori::util::rvalue_reference_wrapper;

class rewrite_scan_test: public ::testing::Test {
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

    std::shared_ptr<storage::index> i0 = storages.add_index("I0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "I0",
    });

    void apply(relation::graph_type& graph) {
        default_index_estimator estimator;
        rewrite_scan(graph, storages, estimator, creator);
    }
};

template<class T, class U = T>
static bool eq(::takatori::util::sequence_view<T const> actual, std::initializer_list<U const> expect) {
    return std::equal(actual.begin(), actual.end(), expect.begin(), expect.end());
}

TEST_F(rewrite_scan_test, stay) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& f0 = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    in.output() >> f0.input();
    f0.output() >> out.input();

    apply(r);

    auto&& result = next<relation::scan>(f0.input());
    EXPECT_EQ(result.source(), bindings(*i0));
}

TEST_F(rewrite_scan_test, point) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& f0 = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    in.output() >> f0.input();
    f0.output() >> out.input();

    auto x0 = storages.add_index("x0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "x0",
            {
                    t0->columns()[0],
            },
    });
    apply(r);

    auto&& result = next<relation::find>(f0.input());
    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 2);
    EXPECT_EQ(result.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(result.columns()[1].source(), bindings(t0c1));
    EXPECT_EQ(result.columns()[0].destination(), c0);
    EXPECT_EQ(result.columns()[1].destination(), c1);

    ASSERT_EQ(result.keys().size(), 1);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(result.keys()[0].value(), constant(0));
}

TEST_F(rewrite_scan_test, range) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& f0 = r.insert(relation::filter {
            land(
                    compare(constant(0), varref(c0), comparison_operator::less_equal),
                    compare(varref(c0), constant(100), comparison_operator::less)),
    });
    in.output() >> f0.input();
    f0.output() >> out.input();

    auto x0 = storages.add_index("x0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "x0",
            {
                    t0->columns()[0],
            },
    });
    apply(r);

    auto&& result = next<relation::scan>(f0.input());
    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 2);
    EXPECT_EQ(result.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(result.columns()[1].source(), bindings(t0c1));
    EXPECT_EQ(result.columns()[0].destination(), c0);
    EXPECT_EQ(result.columns()[1].destination(), c1);

    EXPECT_EQ(result.lower().kind(), relation::endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(result.lower().keys().size(), 1);
    EXPECT_EQ(result.lower().keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(result.lower().keys()[0].value(), constant(0));

    EXPECT_EQ(result.upper().kind(), relation::endpoint_kind::prefixed_exclusive);
    ASSERT_EQ(result.upper().keys().size(), 1);
    EXPECT_EQ(result.upper().keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(result.upper().keys()[0].value(), constant(100));
}

TEST_F(rewrite_scan_test, mixed) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& f0 = r.insert(relation::filter {
            // C0 = 0 AND C1 < 100
            land(
                    compare(varref(c0), constant(0), comparison_operator::equal),
                    compare(varref(c1), constant(100), comparison_operator::less)),
    });
    in.output() >> f0.input();
    f0.output() >> out.input();

    auto x0 = storages.add_index("x0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "x0",
            {
                    t0->columns()[0],
                    t0->columns()[1],
            },
    });
    auto x1 = storages.add_index("x1", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "x1",
            {
                    t0->columns()[1],
                    t0->columns()[0],
            },
    });
    apply(r);

    auto&& result = next<relation::scan>(f0.input());
    EXPECT_EQ(result.source(), bindings(*x0));

    ASSERT_EQ(result.columns().size(), 2);
    EXPECT_EQ(result.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(result.columns()[1].source(), bindings(t0c1));
    EXPECT_EQ(result.columns()[0].destination(), c0);
    EXPECT_EQ(result.columns()[1].destination(), c1);

    EXPECT_EQ(result.lower().kind(), relation::endpoint_kind::prefixed_inclusive);
    ASSERT_EQ(result.lower().keys().size(), 1);
    EXPECT_EQ(result.lower().keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(result.lower().keys()[0].value(), constant(0));

    EXPECT_EQ(result.upper().kind(), relation::endpoint_kind::prefixed_exclusive);
    ASSERT_EQ(result.upper().keys().size(), 2);
    EXPECT_EQ(result.upper().keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(result.upper().keys()[0].value(), constant(0));
    EXPECT_EQ(result.upper().keys()[1].variable(), bindings(t0c1));
    EXPECT_EQ(result.upper().keys()[1].value(), constant(100));
}

TEST_F(rewrite_scan_test, unique) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& f0 = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    in.output() >> f0.input();
    f0.output() >> out.input();

    auto x0 = storages.add_index("x0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "x0",
            {
                    t0->columns()[0],
            },
    });

    auto x1 = storages.add_index("x1", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "x1",
            {
                    t0->columns()[0],
            },
            {},
            {
                    storage::index_feature::find,
                    storage::index_feature::unique,
            },
    });
    apply(r);

    auto&& result = next<relation::find>(f0.input());
    EXPECT_EQ(result.source(), bindings(*x1));

    ASSERT_EQ(result.columns().size(), 2);
    EXPECT_EQ(result.columns()[0].source(), bindings(t0c0));
    EXPECT_EQ(result.columns()[1].source(), bindings(t0c1));
    EXPECT_EQ(result.columns()[0].destination(), c0);
    EXPECT_EQ(result.columns()[1].destination(), c1);

    ASSERT_EQ(result.keys().size(), 1);
    EXPECT_EQ(result.keys()[0].variable(), bindings(t0c0));
    EXPECT_EQ(result.keys()[0].value(), constant(0));
}

TEST_F(rewrite_scan_test, index_only) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c0");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
            },
    });
    auto&& out = r.insert(relation::emit { c0 });

    auto&& f0 = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    in.output() >> f0.input();
    f0.output() >> out.input();

    auto x0 = storages.add_index("x0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "x0",
            {},
            {
                    t0->columns()[0],
                    t0->columns()[1],
                    t0->columns()[2],
            }
    });
    apply(r);

    auto&& result = next<relation::scan>(f0.input());
    EXPECT_EQ(result.source(), bindings(*x0));
}

} // namespace yugawara::analyzer::details
