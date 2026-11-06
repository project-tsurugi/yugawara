#include <yugawara/analyzer/details/rewrite_scan.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/relation/apply.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/join_find.h>
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

    static relation::intermediate::limit limit_desc(std::size_t count, descriptor::variable const& key) {
        return relation::intermediate::limit {
                std::vector<descriptor::variable> {},
                std::vector<relation::intermediate::limit::sort_key> {
                        { key, relation::sort_direction::descendant },
                },
                relation::intermediate::limit::row_slice_type { {}, count },
        };
    }

    static relation::intermediate::limit order_only_desc(descriptor::variable const& key) {
        return relation::intermediate::limit {
                std::vector<descriptor::variable> {},
                std::vector<relation::intermediate::limit::sort_key> {
                        { key, relation::sort_direction::descendant },
                },
        };
    }

    static constexpr runtime_feature_set without_backward_scan {
            runtime_feature::scan_limit,
            runtime_feature::ordered_scan,
            runtime_feature::ordered_forward,
    };

    relation::join_find& add_join_find(relation::graph_type& r, relation::join_kind kind) {
        // NOTE: joins T2 itself by its primary key
        return r.insert(relation::join_find {
                kind,
                bindings(*i2),
                {
                        { t2w, bindings.stream_variable("x") },
                },
                {
                        relation::join_find::key { t2k, varref { w } },
                },
        });
    }

    relation::apply& add_apply(relation::graph_type& r, relation::apply_kind kind, descriptor::variable output) {
        auto&& tvf = bindings.function({
                function::declaration::minimum_user_function_id + 1,
                "tvf",
                ::takatori::type::table {
                        { "o1", ::takatori::type::int8 {} },
                },
                {
                        ::takatori::type::int8 {},
                },
                {
                        function::function_feature::table_valued_function,
                },
        });
        return r.insert(relation::apply {
                tvf,
                {
                        varref { w },
                },
                {
                        { 0, std::move(output) },
                },
                kind,
        });
    }

    static relation::scan::row_slice_type row_slice(std::size_t count) {
        return relation::scan::row_slice_type { {}, count };
    }

    parametric_index_estimator estimator {};
    index_selection_parameters parameters {};

    void apply(
            relation::graph_type& r,
            runtime_feature_set const& features = runtime_feature_all,
            path_capability_options const& capability_options = {}) {
        rewrite_scan(r, estimator, parameters, features, capability_options);
        remove_redundant_conditions(r);
        absorb_limits(r, features, capability_options);
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

TEST_F(rewrite_scan_order_test, early_stop_without_absorb_limits) {
    /*
     * scan:r0{I2} - limit:r1[v, 3] - emit:r2
     * => scan:r0{I2V}[forward, 3] - limit:r1[v, 3] - emit:r2 (by rewrite_scan() only)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(3, v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    // NOTE: rewrite_scan() sets the direction and the row count which contribute to the index selection
    rewrite_scan(r, estimator, parameters, runtime_feature_all, {});

    EXPECT_EQ(r0.source(), bindings(*i2v));
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

TEST_F(rewrite_scan_order_test, early_stop_max_count) {
    /*
     * max_early_stop_limit_count = 10
     * scan:r0{I2} - limit:r1[v, 10] - emit:r2
     * => scan:r0{I2V}[forward, 10] - limit:r1[v, 10] - emit:r2
     */
    parameters.max_early_stop_limit_count() = 10;
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(10, v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_EQ(r0.row_slice(), row_slice(10));
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, early_stop_over_max_count) {
    /*
     * max_early_stop_limit_count = 10
     * scan:r0{I2} - limit:r1[v, 11] - emit:r2
     * => (not changed: same as order_only_indirect, because the row count is too large to stop early)
     */
    parameters.max_early_stop_limit_count() = 10;
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(11, v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, order_over_max_count_without_absorb_limits) {
    /*
     * max_early_stop_limit_count = 10
     * scan:r0{I2} - limit:r1[k, 11] - emit:r2
     * => scan:r0{I2}[forward, 11] - limit:r1[k, 11] - emit:r2 (by rewrite_scan() only)
     */
    parameters.max_early_stop_limit_count() = 10;
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit(11, k));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    // NOTE: the large row count does not contribute to the index selection, but is still set to the scan
    rewrite_scan(r, estimator, parameters, runtime_feature_all, {});

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_EQ(r0.row_slice(), row_slice(11));
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

TEST_F(rewrite_scan_order_test, order_over_join_find_inner) {
    /*
     * scan:r0{I2} - join_find[inner]:r1 - limit:r2[k, 3] - emit:r3
     * => scan:r0{I2}[forward] - join_find[inner]:r1 - limit:r2[k, 3] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = add_join_find(r, relation::join_kind::inner);
    auto&& r2 = r.insert(limit(3, k));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.left();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, order_over_apply_disabled) {
    /*
     * scan:r0{I2} - apply[outer]:r1 - limit:r2[k, 3] - emit:r3
     * the row order is not traced over apply
     */
    relation::graph_type r;
    auto x = bindings.stream_variable("x");
    auto&& r0 = add_t2(r);
    auto&& r1 = add_apply(r, relation::apply_kind::outer, x);
    auto&& r2 = r.insert(limit(3, k));
    auto&& r3 = r.insert(relation::emit { k, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, order_over_apply_outer) {
    /*
     * scan:r0{I2} - apply[outer]:r1 - limit:r2[k, 3] - emit:r3
     * => scan:r0{I2}[forward] - apply[outer]:r1 - limit:r2[k, 3] - emit:r3
     */
    relation::graph_type r;
    auto x = bindings.stream_variable("x");
    auto&& r0 = add_t2(r);
    auto&& r1 = add_apply(r, relation::apply_kind::outer, x);
    auto&& r2 = r.insert(limit(3, k));
    auto&& r3 = r.insert(relation::emit { k, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    path_capability_options capability_options {};
    capability_options.apply_preserves_order = true;
    apply(r, runtime_feature_all, capability_options);

    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(rewrite_scan_order_test, order_over_apply_cross) {
    /*
     * scan:r0{I2} - apply[cross]:r1 - limit:r2[k, 3] - emit:r3
     * => scan:r0{I2}[forward] - apply[cross]:r1 - limit:r2[k, 3] - emit:r3
     * the scan must not stop early, because apply may drop rows
     */
    relation::graph_type r;
    auto x = bindings.stream_variable("x");
    auto&& r0 = add_t2(r);
    auto&& r1 = add_apply(r, relation::apply_kind::cross, x);
    auto&& r2 = r.insert(limit(3, k));
    auto&& r3 = r.insert(relation::emit { k, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    path_capability_options capability_options {};
    capability_options.apply_preserves_order = true;
    apply(r, runtime_feature_all, capability_options);

    EXPECT_EQ(r0.direction(), scan_direction::forward);
    EXPECT_FALSE(r0.row_slice());
}

TEST_F(rewrite_scan_order_test, order_over_apply_output) {
    /*
     * scan:r0{I2} - apply[outer]:r1 - limit:r2[x, 3] - emit:r3
     * the output columns of apply are not provided in the order of the scan
     */
    relation::graph_type r;
    auto x = bindings.stream_variable("x");
    auto&& r0 = add_t2(r);
    auto&& r1 = add_apply(r, relation::apply_kind::outer, x);
    auto&& r2 = r.insert(limit(3, x));
    auto&& r3 = r.insert(relation::emit { k, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    path_capability_options capability_options {};
    capability_options.apply_preserves_order = true;
    apply(r, runtime_feature_all, capability_options);

    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, residual_over_join_find_inner) {
    /*
     * scan:r0{I2} - join_find[inner]:r1 - limit:r2[v, 3] - emit:r3
     * => (not changed: the inner join may drop rows, so that the scan of I2V cannot stop at the row count)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = add_join_find(r, relation::join_kind::inner);
    auto&& r2 = r.insert(limit(3, v));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.left();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, early_stop_over_join_find_left_outer_at_most_one) {
    /*
     * scan:r0{I2} - join_find[left_outer_at_most_one]:r1 - limit:r2[v, 3] - emit:r3
     * => scan:r0{I2V}[forward, 3] - join_find[left_outer_at_most_one]:r1 - limit:r2[v, 3] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = add_join_find(r, relation::join_kind::left_outer_at_most_one);
    auto&& r2 = r.insert(limit(3, v));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.left();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_EQ(r0.row_slice(), row_slice(3));
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

TEST_F(rewrite_scan_order_test, order_only_without_absorb_limits) {
    /*
     * scan:r0{I2} - limit:r1[k] - emit:r2
     * => scan:r0{I2}[forward] - limit:r1[k] - emit:r2 (by rewrite_scan() only)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(order_only(k));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    rewrite_scan(r, estimator, parameters, runtime_feature_all, {});

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

TEST_F(rewrite_scan_order_test, backward_early_stop) {
    /*
     * scan:r0{I2} - limit:r1[k DESC, 3] - emit:r2
     * => scan:r0{I2}[backward, 3] - limit:r1[k DESC, 3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit_desc(3, k));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::backward);
}

TEST_F(rewrite_scan_order_test, backward_early_stop_secondary) {
    /*
     * scan:r0{I2} - limit:r1[v DESC, 3] - emit:r2
     * => scan:r0{I2V}[backward, 3] - limit:r1[v DESC, 3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit_desc(3, v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    // NOTE: the column may contain nulls, but they are the smallest values in both the index keys and the sort keys
    EXPECT_EQ(r0.source(), bindings(*i2v));
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::backward);
}

TEST_F(rewrite_scan_order_test, backward_residual) {
    /*
     * scan:r0{I2} - filter:r1(w = 1) - limit:r2[k DESC, 3] - emit:r3
     * => scan:r0{I2}[backward] - filter:r1(w = 1) - limit:r2[k DESC, 3] - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(relation::filter {
            compare(varref { w }, constant(1)),
    });
    auto&& r2 = r.insert(limit_desc(3, k));
    auto&& r3 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_TRUE(r.contains(r1));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::backward);
}

TEST_F(rewrite_scan_order_test, backward_order_only) {
    /*
     * scan:r0{I2} - limit:r1[k DESC] - emit:r2
     * => scan:r0{I2}[backward] - limit:r1[k DESC] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(order_only_desc(k));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::backward);
}

TEST_F(rewrite_scan_order_test, backward_disabled) {
    /*
     * scan:r0{I2} - limit:r1[k DESC, 3] - emit:r2
     * => (not changed, if backward scan is disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit_desc(3, k));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r, without_backward_scan);

    EXPECT_EQ(r0.source(), bindings(*i2));
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r0.direction());
}

TEST_F(rewrite_scan_order_test, backward_disabled_secondary) {
    /*
     * scan:r0{I2} - limit:r1[v DESC, 3] - emit:r2
     * => (not changed, if backward scan is disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t2(r);
    auto&& r1 = r.insert(limit_desc(3, v));
    auto&& r2 = r.insert(relation::emit { k });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r, without_backward_scan);

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
