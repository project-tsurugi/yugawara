#include <yugawara/analyzer/details/decompose_filter.h>

#include <gtest/gtest.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class decompose_filter_test : public ::testing::Test {
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
    descriptor::variable t0c0 = bindings(t0->columns()[0]);
    descriptor::variable t0c1 = bindings(t0->columns()[1]);
    descriptor::variable t0c2 = bindings(t0->columns()[2]);

    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });

    void apply(relation::graph_type& graph) {
        decompose_filter(graph);
    }
};

TEST_F(decompose_filter_test, simple) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = 0 }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
            compare(varref { c0 }, constant(0)),
    });
    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r0));

    auto&& f0 = next<relation::filter>(r0.output());
    ASSERT_TRUE(r.contains(f0));

    EXPECT_GT(f0.output(), ro.input());

    EXPECT_EQ(f0.condition(), compare(varref { c0 }, constant(0)));
}

TEST_F(decompose_filter_test, conjunction) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = 0 and c1 = 1}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::filter {
            land(
                compare(varref { c0 }, constant(0)),
                compare(varref { c1 }, constant(1))),
    });
    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    ASSERT_TRUE(r.contains(r0));

    auto&& f0 = next<relation::filter>(r0.output());
    ASSERT_TRUE(r.contains(f0));
    EXPECT_EQ(f0.condition(), compare(varref { c0 }, constant(0)));

    auto&& f1 = next<relation::filter>(f0.output());
    ASSERT_TRUE(r.contains(f1));
    EXPECT_EQ(f1.condition(), compare(varref { c1 }, constant(1)));

    EXPECT_GT(f1.output(), ro.input());
}

TEST_F(decompose_filter_test, left_deep) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { (c0 = 0 and c1 = 1) and c2 = 2 }
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
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::filter {
            land(
                land(
                    compare(varref { c0 }, constant(0)),
                    compare(varref { c1 }, constant(1))),
                compare(varref { c2 }, constant(2))),
    });
    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    ASSERT_TRUE(r.contains(r0));

    auto&& f0 = next<relation::filter>(r0.output());
    ASSERT_TRUE(r.contains(f0));
    EXPECT_EQ(f0.condition(), compare(varref { c0 }, constant(0)));

    auto&& f1 = next<relation::filter>(f0.output());
    ASSERT_TRUE(r.contains(f1));
    EXPECT_EQ(f1.condition(), compare(varref { c1 }, constant(1)));

    auto&& f2 = next<relation::filter>(f1.output());
    ASSERT_TRUE(r.contains(f2));
    EXPECT_EQ(f2.condition(), compare(varref { c2 }, constant(2)));

    EXPECT_GT(f2.output(), ro.input());
}

TEST_F(decompose_filter_test, right_deep) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = 0 and (c1 = 1 and c2 = 2) }
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
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::filter {
            land(
                compare(varref { c0 }, constant(0)),
                land(
                    compare(varref { c1 }, constant(1)),
                    compare(varref { c2 }, constant(2)))),
    });
    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    ASSERT_TRUE(r.contains(r0));

    auto&& f0 = next<relation::filter>(r0.output());
    ASSERT_TRUE(r.contains(f0));
    EXPECT_EQ(f0.condition(), compare(varref { c0 }, constant(0)));

    auto&& f1 = next<relation::filter>(f0.output());
    ASSERT_TRUE(r.contains(f1));
    EXPECT_EQ(f1.condition(), compare(varref { c1 }, constant(1)));

    auto&& f2 = next<relation::filter>(f1.output());
    ASSERT_TRUE(r.contains(f2));
    EXPECT_EQ(f2.condition(), compare(varref { c2 }, constant(2)));

    EXPECT_GT(f2.output(), ro.input());
}

TEST_F(decompose_filter_test, disjunction) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = 0 or c1 = 1}
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::filter {
            lor(
                compare(varref { c0 }, constant(0)),
                compare(varref { c1 }, constant(1))),
    });
    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r0));

    auto&& f0 = next<relation::filter>(r0.output());
    ASSERT_TRUE(r.contains(f0));

    EXPECT_GT(f0.output(), ro.input());

    EXPECT_EQ(f0.condition(), lor(
                compare(varref { c0 }, constant(0)),
                compare(varref { c1 }, constant(1))));
}

TEST_F(decompose_filter_test, multiple_filters) {
    /*
     * scan:r0 - filter:r1 - filter:r2 - filter:r3 - emit:ro
     * r1: { c0 > 0 and c0 < 10 }
     * r2: { c1 > 1 and c1 < 11 }
     * r3: { c2 > 2 and c2 < 12 }
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
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::filter {
            land(
                compare(varref { c0 }, constant(0), scalar::comparison_operator::greater),
                compare(varref { c0 }, constant(10), scalar::comparison_operator::less)),
    });
    auto& r2 = r.insert(relation::filter {
            land(
                compare(varref { c1 }, constant(1), scalar::comparison_operator::greater),
                compare(varref { c1 }, constant(11), scalar::comparison_operator::less)),
    });
    auto& r3 = r.insert(relation::filter {
            land(
                compare(varref { c2 }, constant(2), scalar::comparison_operator::greater),
                compare(varref { c2 }, constant(12), scalar::comparison_operator::less)),
    });
    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 8);
    ASSERT_TRUE(r.contains(r0));

    auto&& f0 = next<relation::filter>(r0.output());
    ASSERT_TRUE(r.contains(f0));
    EXPECT_EQ(f0.condition(), compare(varref { c0 }, constant(0), scalar::comparison_operator::greater));

    auto&& f1 = next<relation::filter>(f0.output());
    ASSERT_TRUE(r.contains(f1));
    EXPECT_EQ(f1.condition(), compare(varref { c0 }, constant(10), scalar::comparison_operator::less));

    auto&& f2 = next<relation::filter>(f1.output());
    ASSERT_TRUE(r.contains(f2));
    EXPECT_EQ(f2.condition(), compare(varref { c1 }, constant(1), scalar::comparison_operator::greater));

    auto&& f3 = next<relation::filter>(f2.output());
    ASSERT_TRUE(r.contains(f3));
    EXPECT_EQ(f3.condition(), compare(varref { c1 }, constant(11), scalar::comparison_operator::less));

    auto&& f4 = next<relation::filter>(f3.output());
    ASSERT_TRUE(r.contains(f4));
    EXPECT_EQ(f4.condition(), compare(varref { c2 }, constant(2), scalar::comparison_operator::greater));

    auto&& f5 = next<relation::filter>(f4.output());
    ASSERT_TRUE(r.contains(f5));
    EXPECT_EQ(f5.condition(), compare(varref { c2 }, constant(12), scalar::comparison_operator::less));

    EXPECT_GT(f5.output(), ro.input());
}

} // namespace yugawara::analyzer::details
