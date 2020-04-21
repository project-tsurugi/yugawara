#include <analyzer/details/scan_key_collector.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include "utils.h"

namespace yugawara::analyzer::details {

using ::takatori::scalar::unary;
using ::takatori::scalar::binary;
using ::takatori::scalar::unary_operator;
using ::takatori::scalar::binary_operator;
using ::takatori::scalar::comparison_operator;

using ::takatori::util::object_ownership_reference;
using ::takatori::util::rvalue_reference_wrapper;

class scan_key_collector_test: public ::testing::Test {
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
    std::shared_ptr<storage::table> t1 = storages.add_table("T1", {
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];
    storage::column const& t0c2 = t0->columns()[2];
    storage::column const& t1c0 = t1->columns()[0];
    storage::column const& t1c1 = t1->columns()[1];
    storage::column const& t1c2 = t1->columns()[2];

    std::shared_ptr<storage::index> i0 = storages.add_index("I0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "I0",
    });
    std::shared_ptr<storage::index> i1 = storages.add_index("I1", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t1),
            "I1",
    });
};

template<class T, class U = T>
static bool eq(::takatori::util::sequence_view<T const> actual, std::initializer_list<U const> expect) {
    return std::equal(actual.begin(), actual.end(), expect.begin(), expect.end());
}

TEST_F(scan_key_collector_test, simple) {
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

    scan_key_collector collector { creator };
    ASSERT_TRUE(collector(in));

    auto&& s0 = collector.find(t0c0);
    ASSERT_TRUE(s0);
    EXPECT_EQ(s0->equivalent_factor(), constant(0));

    EXPECT_EQ(collector.table(), *t0);
    EXPECT_TRUE(eq(collector.sort_keys(), {}));
    EXPECT_TRUE(eq(collector.referring(), {
            std::cref(t0c0),
    }));
}

TEST_F(scan_key_collector_test, referring) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& out = r.insert(relation::emit { c0, c1, c2 });

    auto&& f0 = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    in.output() >> f0.input();
    f0.output() >> out.input();

    scan_key_collector collector { creator };
    ASSERT_TRUE(collector(in));

    EXPECT_TRUE(eq(collector.referring(), {
            std::cref(t0c0),
            std::cref(t0c1),
            std::cref(t0c2),
    }));
}

TEST_F(scan_key_collector_test, multiple) {
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& in = r.insert(relation::scan {
            bindings(*i0),
            {
                    { bindings(t0c0), c0 },
                    { bindings(t0c1), c1 },
                    { bindings(t0c2), c2 },
            },
    });
    auto&& out = r.insert(relation::emit { c0, c1, c2 });

    auto&& f0 = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    auto&& f1 = r.insert(relation::filter {
            compare(varref(c1), constant(1)),
    });
    auto&& f2 = r.insert(relation::filter {
            compare(varref(c2), constant(2)),
    });
    in.output() >> f0.input();
    f0.output() >> f1.input();
    f1.output() >> f2.input();
    f2.output() >> out.input();

    scan_key_collector collector { creator };
    ASSERT_TRUE(collector(in));

    EXPECT_EQ(collector.table(), *t0);

    auto&& t0 = collector.find(t0c0);
    auto&& t1 = collector.find(t0c1);
    auto&& t2 = collector.find(t0c2);

    ASSERT_TRUE(t0);
    ASSERT_TRUE(t1);
    ASSERT_TRUE(t2);

    EXPECT_EQ(t0->equivalent_factor(), constant(0));
    EXPECT_EQ(t1->equivalent_factor(), constant(1));
    EXPECT_EQ(t2->equivalent_factor(), constant(2));
}

} // namespace yugawara::analyzer::details
