#include <yugawara/analyzer/details/ordered_relation.h>

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
using ::takatori::relation::sort_direction;

class ordered_relation_test : public ::testing::Test {
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

    relation::scan& add_scan(
            relation::graph_type& r,
            relation::scan::endpoint lower = {},
            relation::scan::endpoint upper = {}) {
        return r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0a, a },
                        { t0b, b },
                        { t0c, c },
                },
                std::move(lower),
                std::move(upper),
        });
    }

    static relation::intermediate::limit limit(std::vector<relation::intermediate::limit::sort_key> sort_keys) {
        return relation::intermediate::limit {
                {},
                std::move(sort_keys),
                relation::intermediate::limit::row_slice_type { {}, 10 },
        };
    }
};

TEST_F(ordered_relation_test, scan) {
    /*
     * scan:r0 - limit:r1 - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit({ a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = find_ordered_source(r1);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.get(), &r0);
}

TEST_F(ordered_relation_test, through_filter_project) {
    /*
     * scan:r0 - filter:r1 - project:r2 - limit:r3 - emit:r4
     */
    relation::graph_type r;
    auto d = bindings.stream_variable("d");
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { c }, constant(1)),
    });
    auto&& r2 = r.insert(relation::project {
            relation::project::column { constant(), d },
    });
    auto&& r3 = r.insert(limit({ a }));
    auto&& r4 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    auto result = find_ordered_source(r3);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.get(), &r0);
}

TEST_F(ordered_relation_test, not_order_preserving) {
    /*
     * scan:r0 - distinct:r1 - limit:r2 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::distinct { a, b });
    auto&& r2 = r.insert(limit({ a }));
    auto&& r3 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    auto result = find_ordered_source(r2);
    EXPECT_FALSE(result);
}

TEST_F(ordered_relation_test, destination) {
    /*
     * scan:r0 - limit:r1 - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit({ a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = find_ordered_destination(r1);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.get(), &r2);
}

TEST_F(ordered_relation_test, destination_through_filter_project) {
    /*
     * scan:r0 - limit:r1 - filter:r2 - project:r3 - emit:r4
     */
    relation::graph_type r;
    auto d = bindings.stream_variable("d");
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit({ a }));
    auto&& r2 = r.insert(relation::filter {
            compare(varref { c }, constant(1)),
    });
    auto&& r3 = r.insert(relation::project {
            relation::project::column { constant(), d },
    });
    auto&& r4 = r.insert(relation::emit { a, d });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    auto result = find_ordered_destination(r1);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.get(), &r4);
}

TEST_F(ordered_relation_test, destination_not_order_preserving) {
    /*
     * scan:r0 - limit:r1 - distinct:r2 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit({ a }));
    auto&& r2 = r.insert(relation::intermediate::distinct { a, b });
    auto&& r3 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    auto result = find_ordered_destination(r1);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.get(), &r2);
}

TEST_F(ordered_relation_test, direction) {
    /*
     * scan:r0 - limit:r1{sort=a, b} - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit({ a, b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    EXPECT_EQ(find_scan_direction(r0, r1), scan_direction::forward);
}

TEST_F(ordered_relation_test, direction_unsatisfied) {
    /*
     * scan:r0 - limit:r1{sort=b} - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit({ b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    EXPECT_FALSE(find_scan_direction(r0, r1));
}

TEST_F(ordered_relation_test, direction_fixed_columns) {
    /*
     * scan:r0{A=1} - limit:r1{sort=b} - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r,
            {
                    relation::scan::key { t0a, constant(1) },
                    relation::endpoint_kind::prefixed_inclusive,
            },
            {
                    relation::scan::key { t0a, constant(1) },
                    relation::endpoint_kind::prefixed_inclusive,
            });
    auto&& r1 = r.insert(limit({ b }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    EXPECT_EQ(find_scan_direction(r0, r1), scan_direction::forward);
}

TEST_F(ordered_relation_test, direction_not_scan_column) {
    /*
     * scan:r0 - project:r1{d} - limit:r2{sort=d} - emit:r3
     */
    relation::graph_type r;
    auto d = bindings.stream_variable("d");
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::project {
            relation::project::column { varref { a }, d },
    });
    auto&& r2 = r.insert(limit({ d }));
    auto&& r3 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    EXPECT_FALSE(find_scan_direction(r0, r2));
}

} // namespace yugawara::analyzer::details
