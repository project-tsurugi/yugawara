#include <yugawara/analyzer/details/absorb_limits.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/limit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/analyzer/details/default_index_estimator.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::relation::endpoint_kind;
using ::takatori::relation::scan_direction;
using ::takatori::relation::sort_direction;

class absorb_limits_test : public ::testing::Test {
protected:
    binding::factory bindings;

    storage::configurable_provider storages;

    // CREATE TABLE T0 (A INT, B INT, C INT, PRIMARY KEY (A, B))
    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "A", t::int4() },
                    { "B", t::int4() },
                    { "C", t::int4() },
            },
    });
    descriptor::variable t0a = bindings(t0->columns()[0]);
    descriptor::variable t0b = bindings(t0->columns()[1]);
    descriptor::variable t0c = bindings(t0->columns()[2]);
    std::shared_ptr<storage::index> i0 = storages.add_index({
            t0,
            "I0",
            {
                    t0->columns()[0],
                    t0->columns()[1],
            },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique,
            },
    });

    // CREATE TABLE T1 (K INT, PRIMARY KEY (K DESC))
    std::shared_ptr<storage::table> t1 = storages.add_table({
            "T1",
            {
                    { "K", t::int4() },
            },
    });
    descriptor::variable t1k = bindings(t1->columns()[0]);
    std::shared_ptr<storage::index> i1 = storages.add_index({
            t1,
            "I1",
            {
                    { t1->columns()[0], storage::sort_direction::descendant },
            },
            {},
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique,
            },
    });


    // CREATE TABLE T2 (K INT PRIMARY KEY, V INT, W INT); CREATE INDEX I2V ON T2 (V)
    std::shared_ptr<storage::table> t2 = storages.add_table({
            "T2",
            {
                    { "K", t::int4() },
                    { "V", t::int4() },
                    { "W", t::int4() },
            },
    });
    descriptor::variable t2k = bindings(t2->columns()[0]);
    descriptor::variable t2v = bindings(t2->columns()[1]);
    descriptor::variable t2w = bindings(t2->columns()[2]);
    std::shared_ptr<storage::index> i2 = storages.add_index({
            t2,
            "I2",
            {
                    t2->columns()[0],
            },
            {
                    t2->columns()[1],
                    t2->columns()[2],
            },
            {
                    storage::index_feature::primary,
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique,
            },
    });
    std::shared_ptr<storage::index> i2v = storages.add_index({
            t2,
            "I2V",
            {
                    t2->columns()[1],
            },
            {},
            {
                    storage::index_feature::find,
                    storage::index_feature::scan,
            },
    });

    descriptor::variable k = bindings.stream_variable("k");
    descriptor::variable v = bindings.stream_variable("v");
    descriptor::variable w = bindings.stream_variable("w");

    relation::scan& add_t2(
            relation::graph_type& r,
            relation::scan::endpoint lower = {},
            relation::scan::endpoint upper = {}) {
        return r.insert(relation::scan {
                bindings(*i2),
                {
                        { t2k, k },
                        { t2v, v },
                        { t2w, w },
                },
                std::move(lower),
                std::move(upper),
        });
    }

    descriptor::variable a = bindings.stream_variable("a");
    descriptor::variable b = bindings.stream_variable("b");
    descriptor::variable c = bindings.stream_variable("c");

    relation::scan& add_t0(
            relation::graph_type& r,
            relation::scan::endpoint lower = {},
            relation::scan::endpoint upper = {},
            relation::scan::direction_type direction = {}) {
        return r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0a, a },
                        { t0b, b },
                        { t0c, c },
                },
                std::move(lower),
                std::move(upper),
                direction,
        });
    }

    static relation::intermediate::limit limit(
            std::size_t count,
            std::vector<relation::intermediate::limit::sort_key> sort_keys = {}) {
        return relation::intermediate::limit {
                {},
                std::move(sort_keys),
                relation::intermediate::limit::row_slice_type { {}, count },
        };
    }

    static relation::scan::row_slice_type row_slice(std::size_t count) {
        return relation::scan::row_slice_type { {}, count };
    }

    default_index_estimator estimator {};

    void apply(relation::graph_type& r, runtime_feature_set const& features = runtime_feature_all) {
        absorb_limits(r, estimator, features);
    }
};

TEST_F(absorb_limits_test, without_sort_keys) {
    /*
     * scan:r0 - limit:r1 - emit:r2
     * => scan:r0[3] - limit:r1 - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_FALSE(r0.direction());
}

TEST_F(absorb_limits_test, zero) {
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(0));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(0));
}

TEST_F(absorb_limits_test, primary_key) {
    /*
     * scan:r0 - limit:r1[a, b] - emit:r2
     * => scan:r0[forward, 3] - limit:r1[a, b] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(3, { a, b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, primary_key_prefix_range) {
    /*
     * scan:r0(a > 2) - limit:r1[a] - emit:r2
     * => scan:r0(a > 2)[forward, 2] - limit:r1[a] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {
            relation::scan::key { t0a, constant(2) },
            endpoint_kind::prefixed_exclusive,
    });
    auto&& r1 = r.insert(limit(2, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, fixed_key) {
    /*
     * scan:r0(a = 1) - limit:r1[b] - emit:r2
     * => scan:r0(a = 1)[forward, 2] - limit:r1[b] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r,
            {
                    relation::scan::key { t0a, constant(1) },
                    endpoint_kind::prefixed_inclusive,
            },
            {
                    relation::scan::key { t0a, constant(1) },
                    endpoint_kind::prefixed_inclusive,
            });
    auto&& r1 = r.insert(limit(2, { b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, descendant_key) {
    /*
     * scan:r0 - limit:r1[k DESC] - emit:r2
     * => scan:r0[forward, 2] - limit:r1[k DESC] - emit:r2
     */
    relation::graph_type r;
    auto k = bindings.stream_variable("k");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1k, k },
            },
    });
    auto&& r1 = r.insert(limit(2, {
            relation::intermediate::limit::sort_key { k, sort_direction::descendant },
    }));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, not_sorted) {
    /*
     * scan:r0 - limit:r1[b] - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(2, { b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(absorb_limits_test, reversed) {
    /*
     * scan:r0 - limit:r1[a DESC] - emit:r2
     * => (never forward)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(2, {
            relation::intermediate::limit::sort_key { a, sort_direction::descendant },
    }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_NE(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, direction_specified) {
    /*
     * scan:r0[forward] - limit:r1[a] - emit:r2
     * => scan:r0[forward, 3] - limit:r1[a] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, {}, scan_direction::forward);
    auto&& r1 = r.insert(limit(3, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, direction_conflict) {
    /*
     * scan:r0[backward] - limit:r1[a] - emit:r2
     * => (never take the row count in the backward direction)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, {}, scan_direction::backward);
    auto&& r1 = r.insert(limit(3, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    // the backward scan provides the last rows in the order of "a", so that it must not take the row count
    EXPECT_TRUE(!r0.row_slice() || r0.direction() == scan_direction::forward);
}

TEST_F(absorb_limits_test, filter) {
    /*
     * scan:r0 - filter:r1 - limit:r2 - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { c }, constant(1)),
    });
    auto&& r2 = r.insert(limit(2));
    auto&& r3 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(absorb_limits_test, join) {
    /*
     * scan:r0 -\
     *           join:r2 - limit:r3 - emit:r4
     * scan:r1 -/
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto k = bindings.stream_variable("k");
    auto&& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1k, k },
            },
    });
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    auto&& r3 = r.insert(limit(2));
    auto&& r4 = r.insert(relation::emit { a });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r1.row_slice());
}

TEST_F(absorb_limits_test, group_keys) {
    /*
     * scan:r0 - limit:r1{a}[b] - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(relation::intermediate::limit {
            { a },
            { b },
            relation::intermediate::limit::row_slice_type { {}, 1 },
    });
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(absorb_limits_test, direction_only) {
    /*
     * scan:r0 - filter:r1 - limit:r2[a] - emit:r3
     * => scan:r0[forward] - filter:r1 - limit:r2[a] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { c }, constant(1)),
    });
    auto&& r2 = r.insert(limit(2, { a }));
    auto&& r3 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, direction_only_disabled) {
    /*
     * scan:r0 - filter:r1 - limit:r2[a] - emit:r3
     * => (not changed, if ordered_forward is disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { c }, constant(1)),
    });
    auto&& r2 = r.insert(limit(2, { a }));
    auto&& r3 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    auto features = runtime_feature_all;
    features.erase(runtime_feature::ordered_forward);
    apply(r, features);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(absorb_limits_test, scan_limit_only) {
    /*
     * scan:r0 - limit:r1[a] - emit:r2
     * => scan:r0[forward, 3] - limit:r1[a] - emit:r2 (only scan_limit is enabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(3, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r, { runtime_feature::scan_limit });

    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, select_index) {
    /*
     * scan:r0{I2} - limit:r1[v] - emit:r2
     * => scan:r0{I2V}[forward, 3] - limit:r1[v] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(3, { v }));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r, { runtime_feature::scan_limit });

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_FALSE(r0.lower());
    EXPECT_FALSE(r0.upper());
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, select_index_with_range) {
    /*
     * scan:r0{I2} - filter:r1(v > 15) - limit:r2[v] - emit:r3
     * => scan:r0{I2V}(v > 15)[forward, 3] - limit:r2[v] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { v }, constant(15), scalar::comparison_operator::greater),
    });
    auto&& r2 = r.insert(limit(3, { v }));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_TRUE(r0.lower());
    EXPECT_FALSE(r0.upper());
    // the filter was removed, because its condition was consumed by the scan range
    EXPECT_EQ(r.size(), 3);
    ASSERT_TRUE(r2.input().opposite());
    EXPECT_EQ(&r2.input().opposite()->owner(), &r0);
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, select_index_direction_only) {
    /*
     * scan:r0{I2} - filter:r1(w = 1) - limit:r2[v] - emit:r3
     * => scan:r0{I2V}[forward] - filter:r1(w = 1) - limit:r2[v] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { w }, constant(1)),
    });
    auto&& r2 = r.insert(limit(3, { v }));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_FALSE(r0.lower());
    EXPECT_FALSE(r0.upper());
    EXPECT_TRUE(r.contains(r1));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, select_index_direction_only_disabled) {
    /*
     * scan:r0{I2} - filter:r1(w = 1) - limit:r2[v] - emit:r3
     * => (not changed, if ordered_forward is disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { w }, constant(1)),
    });
    auto&& r2 = r.insert(limit(3, { v }));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r, { runtime_feature::scan_limit });

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(absorb_limits_test, select_index_scan_range) {
    /*
     * scan:r0{I2}(k > 10) - limit:r1[v] - emit:r2
     * => (not changed, because the scan conditions have decided the index)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r, {
            relation::scan::key { t2k, constant(10) },
            endpoint_kind::prefixed_exclusive,
    });
    auto&& r1 = r.insert(limit(3, { v }));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(absorb_limits_test, select_index_unavailable) {
    /*
     * scan:r0{I2} - limit:r1[w] - emit:r2
     * => (not changed, because no indices provide the order)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(3, { w }));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

} // namespace yugawara::analyzer::details
