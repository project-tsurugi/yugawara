#include <yugawara/analyzer/details/rewrite_scan.h>

#include <gtest/gtest.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/intermediate/limit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/analyzer/parametric_index_estimator.h>

#include <yugawara/analyzer/details/absorb_limits.h>
#include <yugawara/analyzer/details/boolean_constants.h>
#include <yugawara/analyzer/details/remove_redundant_conditions.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::relation::scan_direction;

class rewrite_scan_order_test: public ::testing::Test {
protected:
    binding::factory bindings {};
    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();

    // CREATE TABLE T2 (K INT PRIMARY KEY, V INT, W INT); CREATE INDEX I2V ON T2 (V)
    std::shared_ptr<storage::table> t2 = storages->add_table({
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
    std::shared_ptr<storage::index> i2 = storages->add_index({
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
    std::shared_ptr<storage::index> i2v = storages->add_index({
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

    relation::scan& add_t2(relation::graph_type& r) {
        return r.insert(relation::scan {
                bindings(*i2),
                {
                        { t2k, k },
                        { t2v, v },
                        { t2w, w },
                },
        });
    }

    static relation::intermediate::limit limit(std::size_t count, descriptor::variable const& key) {
        return relation::intermediate::limit {
                std::vector<descriptor::variable> {},
                std::vector<relation::intermediate::limit::sort_key> { key },
                relation::intermediate::limit::row_slice_type { {}, count },
        };
    }

    static relation::intermediate::limit order_only(descriptor::variable const& key) {
        return relation::intermediate::limit {
                std::vector<descriptor::variable> {},
                std::vector<relation::intermediate::limit::sort_key> { key },
        };
    }

    static relation::scan::row_slice_type row_slice(std::size_t count) {
        return relation::scan::row_slice_type { {}, count };
    }

    parametric_index_estimator estimator {};
    index_selection_parameters parameters {};

    void apply(relation::graph_type& r, runtime_feature_set const& features = runtime_feature_all) {
        rewrite_scan(r, estimator, parameters, features);
        remove_redundant_conditions(r);
        absorb_limits(r, features);
    }
};

TEST_F(rewrite_scan_order_test, early_stop) {
    /*
     * scan:r0{I2} - limit:r1[v, 3] - emit:r2
     * => scan:r0{I2V}[forward, 3] - limit:r1[v, 3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(3, v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_FALSE(r0.lower());
    EXPECT_FALSE(r0.upper());
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, early_stop_range) {
    /*
     * scan:r0{I2} - filter:r1(v > 15) - limit:r2[v, 3] - emit:r3
     * => scan:r0{I2V}(v > 15)[forward, 3] - limit:r2[v, 3] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { v }, constant(15), scalar::comparison_operator::greater),
    });
    auto&& r2 = r.insert(limit(3, v));
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
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, early_stop_constant_true) {
    /*
     * scan:r0{I2} - filter:r1(TRUE) - limit:r2[v, 3] - emit:r3
     * => scan:r0{I2V}[forward, 3] - limit:r2[v, 3] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            make_boolean_expression(true),
    });
    auto&& r2 = r.insert(limit(3, v));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    // the constant TRUE is not a residual condition
    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, early_stop_full_bound) {
    /*
     * scan:r0{I2} - filter:r1(v > 15 AND v < 45) - limit:r2[v, 3] - emit:r3
     * => scan:r0{I2V}(15 < v < 45)[forward, 3] - limit:r2[v, 3] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            land(
                    compare(varref { v }, constant(15), scalar::comparison_operator::greater),
                    compare(varref { v }, constant(45), scalar::comparison_operator::less)),
    });
    auto&& r2 = r.insert(limit(3, v));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_TRUE(r0.lower());
    EXPECT_TRUE(r0.upper());
    // the filter was removed, because both terms were consumed by the scan range
    EXPECT_EQ(r.size(), 3);
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, residual_redundant_bound) {
    /*
     * scan:r0{I2} - filter:r1(v > 5 AND v > 15) - limit:r2[v, 3] - emit:r3
     * => (the scan cannot stop at the row count: only one of the lower bounds becomes the scan range)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            land(
                    compare(varref { v }, constant(5), scalar::comparison_operator::greater),
                    compare(varref { v }, constant(15), scalar::comparison_operator::greater)),
    });
    auto&& r2 = r.insert(limit(3, v));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_TRUE(r.contains(r1));
    EXPECT_FALSE(r0.row_slice());
}

TEST_F(rewrite_scan_order_test, residual_after_project) {
    /*
     * scan:r0{I2} - project:r1(x := 1) - filter:r2(v > 15) - limit:r3[v, 3] - emit:r4
     * => (the scan cannot stop at the row count: the filter is out of the scope of search keys)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto x = bindings.stream_variable("x");
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    constant(1), x,
            },
    });
    auto&& r2 = r.insert(relation::filter {
            compare(varref { v }, constant(15), scalar::comparison_operator::greater),
    });
    auto&& r3 = r.insert(limit(3, v));
    auto&& r4 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    EXPECT_TRUE(r.contains(r2));
    EXPECT_FALSE(r0.row_slice());
}

TEST_F(rewrite_scan_order_test, residual_prefers_sort) {
    /*
     * scan:r0{I2} - filter:r1(w = 1) - limit:r2[v, 3] - emit:r3
     * => (not changed: reading I2V in order may read all rows with referring the primary index)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { w }, constant(1)),
    });
    auto&& r2 = r.insert(limit(3, v));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, residual_prefers_order) {
    /*
     * scan:r0{I2} - filter:r1(v > 5) - limit:r2[k, 3] - emit:r3
     * => scan:r0{I2}[forward] - filter:r1(v > 5) - limit:r2[k, 3] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { v }, constant(5), scalar::comparison_operator::greater),
    });
    auto&& r2 = r.insert(limit(3, k));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, order_only_direct) {
    /*
     * scan:r0{I2} - limit:r1[k] - emit:r2
     * => scan:r0{I2}[forward] - limit:r1[k] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(order_only(k));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, order_only_indirect) {
    /*
     * scan:r0{I2} - limit:r1[v] - emit:r2
     * => (not changed: reading I2V in order refers the primary index for all rows)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(order_only(v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, order_only_index_columns) {
    /*
     * scan:r0{I2}(v) - limit:r1[v] - emit:r2
     * => scan:r0{I2V}(v)[forward] - limit:r1[v] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = r.insert(relation::scan {
            bindings(*i2),
            {
                    { t2v, v },
            },
    });
    auto&& r1 = r.insert(order_only(v));
    auto&& r2 = r.insert(relation::emit { v });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, disabled) {
    /*
     * scan:r0{I2} - limit:r1[v, 3] - emit:r2
     * => (not changed, if the TOP-N features are disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(3, v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r, {});

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, scan_limit_only_with_residual) {
    /*
     * scan:r0{I2} - filter:r1(w = 1) - limit:r2[v, 3] - emit:r3
     * => (not changed: scan_limit cannot stop the scan with the residual filter)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { w }, constant(1)),
    });
    auto&& r2 = r.insert(limit(3, v));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r, { runtime_feature::scan_limit });

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.direction());
}

} // namespace yugawara::analyzer::details
