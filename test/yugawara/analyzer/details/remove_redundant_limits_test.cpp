#include <yugawara/analyzer/details/remove_redundant_limits.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::relation::scan_direction;

class remove_redundant_limits_test : public ::testing::Test {
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

    descriptor::variable a = bindings.stream_variable("a");
    descriptor::variable b = bindings.stream_variable("b");
    descriptor::variable c = bindings.stream_variable("c");

    relation::scan& add_t0(
            relation::graph_type& r,
            relation::scan::direction_type direction = {},
            std::optional<std::size_t> count = {}) {
        auto&& result = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0a, a },
                        { t0b, b },
                        { t0c, c },
                },
                {},
                {},
                direction,
        });
        if (count) {
            result.row_slice() = relation::scan::row_slice_type { {}, *count };
        }
        return result;
    }

    static relation::intermediate::limit limit(
            std::optional<std::size_t> count,
            std::vector<relation::intermediate::limit::sort_key> sort_keys = {}) {
        relation::intermediate::limit::row_slice_type slice {};
        if (count) {
            slice = relation::intermediate::limit::row_slice_type { {}, *count };
        }
        return relation::intermediate::limit {
                std::vector<descriptor::variable> {},
                std::move(sort_keys),
                std::move(slice),
        };
    }

    static void apply(relation::graph_type& r, runtime_feature_set const& features = runtime_feature_all) {
        remove_redundant_limits(r, features);
    }
};

TEST_F(remove_redundant_limits_test, count) {
    /*
     * scan:r0[3] - limit:r1(3) - emit:r2
     * => scan:r0[3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, 3);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, count_less) {
    /*
     * scan:r0[3] - limit:r1(5) - emit:r2
     * => scan:r0[3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, 3);
    auto&& r1 = r.insert(limit(5));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, count_greater) {
    /*
     * scan:r0[5] - limit:r1(3) - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, 5);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, count_without_scan_count) {
    /*
     * scan:r0 - limit:r1(3) - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, count_disabled) {
    /*
     * scan:r0[3] - limit:r1(3) - emit:r2
     * => (not changed, if scan_limit is disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, 3);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto features = runtime_feature_all;
    features.erase(runtime_feature::scan_limit);
    apply(r, features);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, ordered) {
    /*
     * scan:r0[forward, 3] - limit:r1[a](3) - emit:r2
     * => scan:r0[forward, 3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, scan_direction::forward, 3);
    auto&& r1 = r.insert(limit(3, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, ordered_without_count) {
    /*
     * scan:r0[forward] - limit:r1[a, b] - emit:r2
     * => scan:r0[forward] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, scan_direction::forward);
    auto&& r1 = r.insert(limit({}, { a, b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, ordered_without_direction) {
    /*
     * scan:r0[3] - limit:r1[a](3) - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, 3);
    auto&& r1 = r.insert(limit(3, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, ordered_unsatisfied) {
    /*
     * scan:r0[forward, 3] - limit:r1[b](3) - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, scan_direction::forward, 3);
    auto&& r1 = r.insert(limit(3, { b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, ordered_disabled) {
    /*
     * scan:r0[forward, 3] - limit:r1[a](3) - emit:r2
     * => (not changed, if ordered_scan is disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, scan_direction::forward, 3);
    auto&& r1 = r.insert(limit(3, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto features = runtime_feature_all;
    features.erase(runtime_feature::ordered_scan);
    apply(r, features);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, through_filter_project) {
    /*
     * scan:r0[forward, 3] - filter:r1 - project:r2 - limit:r3[a](3) - emit:r4
     * => scan:r0[forward, 3] - filter:r1 - project:r2 - emit:r4
     */
    relation::graph_type r;
    auto x = bindings.stream_variable("x");
    auto&& r0 = add_t0(r, scan_direction::forward, 3);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { c }, constant(1)),
    });
    auto&& r2 = r.insert(relation::project {
            relation::project::column { constant(), x },
    });
    auto&& r3 = r.insert(limit(3, { a }));
    auto&& r4 = r.insert(relation::emit { a, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r2.output(), r4.input());
}

TEST_F(remove_redundant_limits_test, start) {
    /*
     * scan:r0[3] - limit:r1(2, 3) - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, 3);
    auto&& r1 = r.insert(relation::intermediate::limit {
            relation::intermediate::limit::row_slice_type { 2, 3 },
    });
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, group_keys) {
    /*
     * scan:r0[forward] - limit:r1{a}[b] - emit:r2
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, scan_direction::forward);
    auto&& r1 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> { a },
            std::vector<relation::intermediate::limit::sort_key> { b },
    });
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(remove_redundant_limits_test, empty) {
    /*
     * scan:r0 - distinct:r1 - limit:r2 - emit:r3
     * => scan:r0 - distinct:r1 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(relation::intermediate::distinct { a });
    auto&& r2 = r.insert(limit({}));
    auto&& r3 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r, {});

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r3.input());
}

} // namespace yugawara::analyzer::details
