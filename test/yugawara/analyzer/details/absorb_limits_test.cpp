#include <yugawara/analyzer/details/absorb_limits.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/project.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>


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

    void apply(relation::graph_type& r, runtime_feature_set const& features = runtime_feature_all) {
        absorb_limits(r, features);
    }

    /**
     * @brief adds an apply which calls a table-valued function with `b`, and outputs the column `x`.
     */
    relation::apply& add_apply(relation::graph_type& r, relation::apply_kind kind, descriptor::variable const& x) {
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
                        varref { b },
                },
                {
                        { 0, x },
                },
                kind,
        });
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

TEST_F(absorb_limits_test, sort_keys) {
    /*
     * scan:r0[forward] - limit:r1[a, 3] - emit:r2
     * => (not changed: the limits with sort keys are handled by rewrite_scan())
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, {}, scan_direction::forward);
    auto&& r1 = r.insert(limit(3, { a }));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r0.direction(), scan_direction::forward);
}

TEST_F(absorb_limits_test, with_direction) {
    /*
     * scan:r0[backward] - limit:r1[3] - emit:r2
     * => scan:r0[backward, 3] - limit:r1[3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r, {}, {}, scan_direction::backward);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    // the limit without sort keys accepts any rows
    EXPECT_EQ(r0.row_slice(), row_slice(3));
    EXPECT_EQ(r0.direction(), scan_direction::backward);
}

TEST_F(absorb_limits_test, scan_count_smaller) {
    /*
     * scan:r0[2] - limit:r1[3] - emit:r2
     * => scan:r0[2] - limit:r1[3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    r0.row_slice() = row_slice(2);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
}

TEST_F(absorb_limits_test, scan_count_larger) {
    /*
     * scan:r0[5] - limit:r1[3] - emit:r2
     * => scan:r0[3] - limit:r1[3] - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    r0.row_slice() = row_slice(5);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(3));
}

TEST_F(absorb_limits_test, scan_limit_disabled) {
    /*
     * scan:r0 - limit:r1[3] - emit:r2
     * => (not changed: runtime_feature::scan_limit is disabled)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = r.insert(limit(3));
    auto&& r2 = r.insert(relation::emit { a });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r, { runtime_feature::ordered_scan });

    EXPECT_FALSE(r0.row_slice());
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

TEST_F(absorb_limits_test, project) {
    /*
     * scan:r0 - project:r1 - limit:r2 - emit:r3
     * => scan:r0[2] - project:r1 - limit:r2 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto x = bindings.stream_variable("x");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { constant(1), x },
    });
    auto&& r2 = r.insert(limit(2));
    auto&& r3 = r.insert(relation::emit { a, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    // each row of the scan becomes exactly one row of the project
    EXPECT_EQ(r0.row_slice(), row_slice(2));
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

TEST_F(absorb_limits_test, left_outer_join) {
    /*
     * scan:r0 -\
     *           join[left_outer]:r2 - limit:r3 - emit:r4
     * scan:r1 -/
     * => scan:r0[2] - ...
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
            relation::join_kind::left_outer,
    });
    auto&& r3 = r.insert(limit(2));
    auto&& r4 = r.insert(relation::emit { a });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    // NOTE: each left input row becomes one or more output rows
    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_FALSE(r1.row_slice());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(absorb_limits_test, outer_apply) {
    /*
     * scan:r0 - apply[outer]:r1 - limit:r2 - emit:r3
     * => scan:r0[2] - apply[outer]:r1 - limit:r2 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto x = bindings.stream_variable("x");
    auto&& r1 = add_apply(r, relation::apply_kind::outer, x);
    auto&& r2 = r.insert(limit(2));
    auto&& r3 = r.insert(relation::emit { a, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(absorb_limits_test, cross_apply) {
    /*
     * scan:r0 - apply[cross]:r1 - limit:r2 - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto x = bindings.stream_variable("x");
    auto&& r1 = add_apply(r, relation::apply_kind::cross, x);
    auto&& r2 = r.insert(limit(2));
    auto&& r3 = r.insert(relation::emit { a, x });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    // NOTE: the table-valued function may not return any rows for some input rows
    EXPECT_FALSE(r0.row_slice());
}

TEST_F(absorb_limits_test, left_outer_join_over_project) {
    /*
     * scan:r0 - project:rp -\
     *                        join[left_outer]:r2 - apply[outer]:ra - limit:r3 - emit:r4
     * scan:r1 --------------/
     * => scan:r0[2] - ...
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto y = bindings.stream_variable("y");
    auto&& rp = r.insert(relation::project {
            relation::project::column { constant(), y },
    });
    auto k = bindings.stream_variable("k");
    auto&& r1 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1k, k },
            },
    });
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto x = bindings.stream_variable("x");
    auto&& ra = add_apply(r, relation::apply_kind::outer, x);
    auto&& r3 = r.insert(limit(2));
    auto&& r4 = r.insert(relation::emit { a, x, y });
    r0.output() >> rp.input();
    rp.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> ra.input();
    ra.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
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

TEST_F(absorb_limits_test, union_all) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 - limit:r3 - emit:r4
     * => scan:r0[2], scan:r1[2]
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = add_t2(r);
    auto u = bindings.stream_variable("u");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { a, k, u },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit(2));
    auto&& r4 = r.insert(relation::emit { u });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    // NOTE: each input row of union all becomes exactly one output row
    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_EQ(r1.row_slice(), row_slice(2));
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(absorb_limits_test, union_distinct) {
    /*
     * scan:r0 -\
     * scan:r1 - union[distinct]:r2 - limit:r3 - emit:r4
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = add_t2(r);
    auto u = bindings.stream_variable("u");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { a, k, u },
            },
            relation::set_quantifier::distinct,
    });
    auto&& r3 = r.insert(limit(2));
    auto&& r4 = r.insert(relation::emit { u });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    // NOTE: union distinct may drop the duplicated input rows
    EXPECT_FALSE(r0.row_slice());
    EXPECT_FALSE(r1.row_slice());
}

TEST_F(absorb_limits_test, union_all_filter) {
    /*
     * scan:r0 - filter:rf -\
     * scan:r1 ------------- union[all]:r2 - limit:r3 - emit:r4
     * => scan:r1[2]
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& rf = r.insert(relation::filter {
            compare(varref { c }, constant(1)),
    });
    auto&& r1 = add_t2(r);
    auto u = bindings.stream_variable("u");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { a, k, u },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit(2));
    auto&& r4 = r.insert(relation::emit { u });
    r0.output() >> rf.input();
    rf.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    EXPECT_FALSE(r0.row_slice());
    EXPECT_EQ(r1.row_slice(), row_slice(2));
}

TEST_F(absorb_limits_test, union_all_nested) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 -\
     * scan:r3 ----------------- union[all]:r4 - limit:r5 - emit:r6
     * => scan:r0[2], scan:r1[2], scan:r3[2]
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = add_t2(r);
    auto u = bindings.stream_variable("u");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { a, k, u },
            },
            relation::set_quantifier::all,
    });
    auto x = bindings.stream_variable("x");
    auto&& r3 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1k, x },
            },
    });
    auto y = bindings.stream_variable("y");
    auto&& r4 = r.insert(relation::intermediate::union_ {
            {
                    { u, x, y },
            },
            relation::set_quantifier::all,
    });
    auto&& r5 = r.insert(limit(2));
    auto&& r6 = r.insert(relation::emit { y });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r4.left();
    r3.output() >> r4.right();
    r4.output() >> r5.input();
    r5.output() >> r6.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_EQ(r1.row_slice(), row_slice(2));
    EXPECT_EQ(r3.row_slice(), row_slice(2));
}

TEST_F(absorb_limits_test, left_outer_join_union_all) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 -\
     * scan:r3 ----------------- join[left_outer]:r4 - limit:r5 - emit:r6
     * => scan:r0[2], scan:r1[2]
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = add_t2(r);
    auto u = bindings.stream_variable("u");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { a, k, u },
            },
            relation::set_quantifier::all,
    });
    auto x = bindings.stream_variable("x");
    auto&& r3 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1k, x },
            },
    });
    auto&& r4 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r5 = r.insert(limit(2));
    auto&& r6 = r.insert(relation::emit { u, x });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r4.left();
    r3.output() >> r4.right();
    r4.output() >> r5.input();
    r5.output() >> r6.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_EQ(r1.row_slice(), row_slice(2));
    EXPECT_FALSE(r3.row_slice());
}

TEST_F(absorb_limits_test, union_all_smaller_count) {
    /*
     * scan:r0 -----\
     * scan:r1[1] - union[all]:r2 - limit:r3 - emit:r4
     * => scan:r0[2], scan:r1[1]
     */
    relation::graph_type r;
    auto&& r0 = add_t0(r);
    auto&& r1 = add_t2(r);
    r1.row_slice() = row_slice(1);
    auto u = bindings.stream_variable("u");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { a, k, u },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit(2));
    auto&& r4 = r.insert(relation::emit { u });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    EXPECT_EQ(r0.row_slice(), row_slice(2));
    EXPECT_EQ(r1.row_slice(), row_slice(1));
}

} // namespace yugawara::analyzer::details
