#include <yugawara/analyzer/details/push_down_limits.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/scalar/binary.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/relation/fence.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class push_down_limits_test : public ::testing::Test {
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

    descriptor::variable c0 = bindings.stream_variable("c0");
    descriptor::variable c1 = bindings.stream_variable("c1");
    descriptor::variable c2 = bindings.stream_variable("c2");

    std::shared_ptr<storage::table> t1 = storages.add_table({
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
            },
    });
    descriptor::variable t1c0 = bindings(t1->columns()[0]);
    descriptor::variable t1c1 = bindings(t1->columns()[1]);

    std::shared_ptr<storage::index> i1 = storages.add_index({ t1, "I1", });

    descriptor::variable d0 = bindings.stream_variable("d0");
    descriptor::variable d1 = bindings.stream_variable("d1");

    std::size_t max_partial_limit_count = 100;

    relation::scan& add_scan(relation::graph_type& r) {
        return r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                        { t0c2, c2 },
                },
        });
    }

    relation::scan& add_scan1(relation::graph_type& r) {
        return r.insert(relation::scan {
                bindings(*i1),
                {
                        { t1c0, d0 },
                        { t1c1, d1 },
                },
        });
    }

    static scalar::binary plus(descriptor::variable const& variable, v::int4::entity_type value) {
        return scalar::binary {
                scalar::binary_operator::add,
                varref { variable },
                constant(value),
        };
    }

    static relation::intermediate::limit limit(
            std::vector<relation::intermediate::limit::sort_key> sort_keys = {}) {
        return relation::intermediate::limit {
                {},
                std::move(sort_keys),
                relation::intermediate::limit::row_slice_type { {}, 3 },
        };
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

    static relation::intermediate::limit::row_slice_type row_slice(std::size_t count) {
        return relation::intermediate::limit::row_slice_type { {}, count };
    }

    void apply(relation::graph_type& r) const {
        push_down_limits(r, max_partial_limit_count, {});
    }

    static ::takatori::util::optional_ptr<relation::intermediate::limit> upstream_limit(
            relation::expression::input_port_type& port) {
        auto upstream = port.opposite();
        if (!upstream || upstream->owner().kind() != relation::intermediate::limit::tag) {
            return {};
        }
        return ::takatori::util::unsafe_downcast<relation::intermediate::limit>(upstream->owner());
    }

    /**
     * @brief adds an apply which calls a table-valued function with `c1`, and outputs the column `x0`.
     */
    relation::apply& add_apply(relation::graph_type& r, relation::apply_kind kind, descriptor::variable const& x0) {
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
                        varref { c1 },
                },
                {
                        { 0, x0 },
                },
                kind,
        });
    }
};

TEST_F(push_down_limits_test, project) {
    /*
     * scan:r0 - project:r1 - limit:r2 - emit:r3
     * =>
     * scan:r0 - limit:r2 - project:r1 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto&& r2 = r.insert(limit());
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r2.input());
    EXPECT_GT(r2.output(), r1.input());
    EXPECT_GT(r1.output(), r3.input());
}

TEST_F(push_down_limits_test, project_sort_key_not_defined) {
    /*
     * scan:r0 - project:r1 - limit:r2[c1] - emit:r3
     * =>
     * scan:r0 - limit:r2[c1] - project:r1 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto&& r2 = r.insert(limit({ relation::intermediate::limit::sort_key { c1 } }));
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r2.input());
    EXPECT_GT(r2.output(), r1.input());
    EXPECT_GT(r1.output(), r3.input());
}

TEST_F(push_down_limits_test, project_chain) {
    /*
     * scan:r0 - project:r1 - project:r2 - limit:r3 - emit:r4
     * =>
     * scan:r0 - limit:r3 - project:r1 - project:r2 - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto x1 = bindings.stream_variable("x1");
    auto&& r2 = r.insert(relation::project {
            relation::project::column { x1, plus(x0, 1) },
    });
    auto&& r3 = r.insert(limit());
    auto&& r4 = r.insert(relation::emit { x1 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r3.input());
    EXPECT_GT(r3.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r4.input());
}

TEST_F(push_down_limits_test, project_sort_key_defined) {
    /*
     * scan:r0 - project:r1[s0 := c0 + 1] - limit:r2[s0] - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto s0 = bindings.stream_variable("s0");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { s0, plus(c0, 1) },
    });
    auto&& r2 = r.insert(limit({ relation::intermediate::limit::sort_key { s0 } }));
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, project_chain_sort_key_defined) {
    /*
     * scan:r0 - project:r1[s0 := c0 + 1] - project:r2 - limit:r3[s0] - emit:r4
     * =>
     * scan:r0 - project:r1[s0 := c0 + 1] - limit:r3[s0] - project:r2 - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto s0 = bindings.stream_variable("s0");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { s0, plus(c0, 1) },
    });
    auto x0 = bindings.stream_variable("x0");
    auto&& r2 = r.insert(relation::project {
            relation::project::column { x0, plus(c1, 1) },
    });
    auto&& r3 = r.insert(limit({ relation::intermediate::limit::sort_key { s0 } }));
    auto&& r4 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_GT(r3.output(), r2.input());
    EXPECT_GT(r2.output(), r4.input());
}

TEST_F(push_down_limits_test, filter) {
    /*
     * scan:r0 - filter:r1 - limit:r2 - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::filter {
            compare(c0, c1),
    });
    auto&& r2 = r.insert(limit());
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, project_over_filter) {
    /*
     * scan:r0 - filter:r1 - project:r2 - limit:r3 - emit:r4
     * =>
     * scan:r0 - filter:r1 - limit:r3 - project:r2 - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::filter {
            compare(c0, c1),
    });
    auto x0 = bindings.stream_variable("x0");
    auto&& r2 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto&& r3 = r.insert(limit());
    auto&& r4 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_GT(r3.output(), r2.input());
    EXPECT_GT(r2.output(), r4.input());
}

TEST_F(push_down_limits_test, distinct) {
    /*
     * scan:r0 - distinct:r1 - limit:r2 - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::distinct {
            c0,
    });
    auto&& r2 = r.insert(limit());
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, merge) {
    /*
     * scan:r0 - limit:r1[c0](10) - limit:r2[c0](3) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0](3) - limit:r2[c0](3) - emit:r3
     * (the downstream limit is removed later)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(10, { c0 }));
    auto&& r2 = r.insert(limit(3, { c0 }));
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    EXPECT_EQ(r2.row_slice(), row_slice(3));
    ASSERT_EQ(r1.sort_keys().size(), 1);
    EXPECT_EQ(r1.sort_keys()[0].variable(), c0);
}

TEST_F(push_down_limits_test, merge_prefix) {
    /*
     * scan:r0 - limit:r1[c0, c1](3) - limit:r2[c0](10) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0, c1](3) - limit:r2[c0](10) - emit:r3
     * (the downstream limit is removed later)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(3, { c0, c1 }));
    auto&& r2 = r.insert(limit(10, { c0 }));
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    ASSERT_EQ(r1.sort_keys().size(), 2);
}

TEST_F(push_down_limits_test, merge_without_sort_keys) {
    /*
     * scan:r0 - limit:r1[c0](10) - limit:r2(3) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0](3) - limit:r2(3) - emit:r3
     * (the downstream limit is removed later)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(10, { c0 }));
    auto&& r2 = r.insert(limit(3));
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    ASSERT_EQ(r1.sort_keys().size(), 1);
}

TEST_F(push_down_limits_test, merge_inner_without_count) {
    /*
     * scan:r0 - limit:r1[c0] - limit:r2[c0](3) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0](3) - limit:r2[c0](3) - emit:r3
     * (the downstream limit is removed later)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
    });
    auto&& r2 = r.insert(limit(3, { c0 }));
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
}

TEST_F(push_down_limits_test, merge_over_project) {
    /*
     * scan:r0 - limit:r1[c0](10) - project:r2 - limit:r3[c0](3) - emit:r4
     * =>
     * scan:r0 - limit:r1[c0](3) - limit:r3[c0](3) - project:r2 - emit:r4
     * (the downstream limit is removed later)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(limit(10, { c0 }));
    auto&& r2 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto&& r3 = r.insert(limit(3, { c0 }));
    auto&& r4 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_GT(r3.output(), r2.input());
    EXPECT_GT(r2.output(), r4.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
}

TEST_F(push_down_limits_test, merge_chain) {
    /*
     * scan:r0 - limit:r1[c0](10) - limit:r2[c0](5) - limit:r3(3) - emit:r4
     * =>
     * scan:r0 - limit:r1[c0](3) - limit:r2[c0](3) - limit:r3(3) - emit:r4
     * (the row count of the downstream is passed to the farthest upstream, and the downstream ones are removed later)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(10, { c0 }));
    auto&& r2 = r.insert(limit(5, { c0 }));
    auto&& r3 = r.insert(limit(3));
    auto&& r4 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_GT(r3.output(), r4.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    EXPECT_EQ(r2.row_slice(), row_slice(3));
}

TEST_F(push_down_limits_test, merge_inconsistent_sort_keys) {
    /*
     * scan:r0 - limit:r1[c0](10) - limit:r2[c1](3) - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(10, { c0 }));
    auto&& r2 = r.insert(limit(3, { c1 }));
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, merge_start) {
    /*
     * scan:r0 - limit:r1[c0](2, 10) - limit:r2[c0](3) - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::limit {
            {},
            { c0 },
            relation::intermediate::limit::row_slice_type { 2, 10 },
    });
    auto&& r2 = r.insert(limit(3, { c0 }));
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, merge_same_variable_count) {
    /*
     * scan:r0 - limit:r1[c0](n) - limit:r2[c0](n) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0](n) - limit:r2[c0](n) - emit:r3
     * (the downstream limit is removed later)
     */
    relation::graph_type r;
    auto n = bindings.stream_variable("n");
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { {}, n },
    });
    auto&& r2 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { {}, n },
    });
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), (relation::intermediate::limit::row_slice_type { {}, n }));
}

TEST_F(push_down_limits_test, merge_different_variable_count) {
    /*
     * scan:r0 - limit:r1[c0](10) - limit:r2[c0](n) - emit:r3
     * => (not changed)
     */
    relation::graph_type r;
    auto n = bindings.stream_variable("n");
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(10, { c0 }));
    auto&& r2 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { {}, n },
    });
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, buffer_stop_with_primary) {
    /*
     * scan:r0 - buffer[stop_with_primary]:r1 -+- limit:r2[c0](3) - emit:r3
     *                                          +- emit:r4
     * =>
     * scan:r0 - limit:r2[c0](3) - buffer[stop_with_primary]:r1 -+- emit:r3
     *                                                            +- emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::buffer { 2, true });
    auto&& r2 = r.insert(limit({ c0 }));
    auto&& r3 = r.insert(relation::emit { c0 });
    auto&& r4 = r.insert(relation::emit { c1 });
    r0.output() >> r1.input();
    r1.primary_output() >> r2.input();
    r2.output() >> r3.input();
    r1.secondary_outputs()[0] >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r2.input());
    EXPECT_GT(r2.output(), r1.input());
    EXPECT_GT(r1.primary_output(), r3.input());
    EXPECT_GT(r1.secondary_outputs()[0], r4.input());
}

TEST_F(push_down_limits_test, buffer_without_stop_with_primary) {
    /*
     * scan:r0 - buffer:r1 -+- limit:r2[c0](3) - emit:r3
     *                       +- emit:r4
     * =>
     * (unchanged)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::buffer { 2 });
    auto&& r2 = r.insert(limit({ c0 }));
    auto&& r3 = r.insert(relation::emit { c0 });
    auto&& r4 = r.insert(relation::emit { c1 });
    r0.output() >> r1.input();
    r1.primary_output() >> r2.input();
    r2.output() >> r3.input();
    r1.secondary_outputs()[0] >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.primary_output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_GT(r1.secondary_outputs()[0], r4.input());
}

TEST_F(push_down_limits_test, buffer_secondary_output) {
    /*
     * scan:r0 - buffer[stop_with_primary]:r1 -+- emit:r3
     *                                          +- limit:r2[c0](3) - emit:r4
     * =>
     * (unchanged)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::buffer { 2, true });
    auto&& r2 = r.insert(limit({ c0 }));
    auto&& r3 = r.insert(relation::emit { c1 });
    auto&& r4 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.primary_output() >> r3.input();
    r1.secondary_outputs()[0] >> r2.input();
    r2.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.primary_output(), r3.input());
    EXPECT_GT(r1.secondary_outputs()[0], r2.input());
    EXPECT_GT(r2.output(), r4.input());
}

TEST_F(push_down_limits_test, buffer_partial_limit) {
    /*
     * scan:r0 - buffer[stop_with_primary]:r1 -+------------------------------------------------ emit:r6
     *                                          \
     *                               scan:r5 --- join[left_outer_at_most_one]:r2 - limit:r3[c0](3) - emit:r4
     * =>
     * scan:r0 - limit:p[c0](3) - buffer[stop_with_primary]:r1 -+------------------------------------------------ emit:r6
     *                                                           \
     *                                                scan:r5 --- join[left_outer_at_most_one]:r2 - limit:r3[c0](3) - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::buffer { 2, true });
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer_at_most_one,
    });
    auto&& r3 = r.insert(limit({ c0 }));
    auto&& r4 = r.insert(relation::emit { c0, d0 });
    auto&& r5 = add_scan1(r);
    auto&& r6 = r.insert(relation::emit { c1 });
    r0.output() >> r1.input();
    r1.primary_output() >> r2.left();
    r5.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();
    r1.secondary_outputs()[0] >> r6.input();

    apply(r);

    ASSERT_EQ(r.size(), 8);
    auto p = upstream_limit(r1.input());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->group_keys().size(), 0);
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_EQ(p->row_slice(), row_slice(3));
    EXPECT_GT(r1.primary_output(), r2.left());
    EXPECT_GT(r1.secondary_outputs()[0], r6.input());
    EXPECT_GT(r5.output(), r2.right());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, left_outer_join) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 - limit:r3[c0](3) - emit:r4
     * =>
     * scan:r0 - limit:p[c0](3) -\
     * scan:r1 ------------------ join[left_outer]:r2 - limit:r3[c0](3) - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r3 = r.insert(limit({ c0 }));
    auto&& r4 = r.insert(relation::emit { c0, d0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto p = upstream_limit(r2.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->group_keys().size(), 0);
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_EQ(p->row_slice(), row_slice(3));
    EXPECT_GT(r1.output(), r2.right());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, left_outer_at_most_one_join) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer_at_most_one]:r2 - limit:r3[c0](3) - emit:r4
     * =>
     * scan:r0 - limit:p[c0](3) -\
     * scan:r1 ------------------ join[left_outer_at_most_one]:r2 - limit:r3[c0](3) - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer_at_most_one,
    });
    auto&& r3 = r.insert(limit({ c0 }));
    auto&& r4 = r.insert(relation::emit { c0, d0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto p = upstream_limit(r2.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_EQ(p->row_slice(), row_slice(3));
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, join_without_partial_limits) {
    /*
     * scan:r0 -\
     * scan:r1 - join[*]:r2 - limit:r3[c0](3) - emit:r4
     * => (not changed)
     */
    for (auto kind : {
            relation::join_kind::inner,
            relation::join_kind::full_outer,
            relation::join_kind::semi,
            relation::join_kind::anti,
    }) {
        relation::graph_type r;
        auto&& r0 = add_scan(r);
        auto&& r1 = add_scan1(r);
        auto&& r2 = r.insert(relation::intermediate::join { kind });
        auto&& r3 = r.insert(limit({ c0 }));
        auto&& r4 = r.insert(relation::emit { c0 });
        r0.output() >> r2.left();
        r1.output() >> r2.right();
        r2.output() >> r3.input();
        r3.output() >> r4.input();

        apply(r);

        ASSERT_EQ(r.size(), 5) << kind;
        EXPECT_GT(r0.output(), r2.left()) << kind;
        EXPECT_GT(r1.output(), r2.right()) << kind;
        EXPECT_GT(r2.output(), r3.input()) << kind;
    }
}

TEST_F(push_down_limits_test, left_outer_join_right_sort_key) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 - limit:r3[c0, d0](3) - emit:r4
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r3 = r.insert(limit({ c0, d0 }));
    auto&& r4 = r.insert(relation::emit { c0, d0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r2.left());
    EXPECT_GT(r1.output(), r2.right());
}

TEST_F(push_down_limits_test, left_outer_join_variable_count) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 - limit:r3[c0](n) - emit:r4
     * => (not changed)
     */
    relation::graph_type r;
    auto n = bindings.stream_variable("n");
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r3 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { {}, n },
    });
    auto&& r4 = r.insert(relation::emit { c0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r2.left());
}

TEST_F(push_down_limits_test, left_outer_join_start) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 - limit:r3[c0](1, 3) - emit:r4
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r3 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> {},
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            relation::intermediate::limit::row_slice_type { 1, 3 },
    });
    auto&& r4 = r.insert(relation::emit { c0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r2.left());
}

TEST_F(push_down_limits_test, left_outer_join_group_keys) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 - limit:r3{c1}[c0](3) - emit:r4
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r3 = r.insert(relation::intermediate::limit {
            std::vector<descriptor::variable> { c1 },
            std::vector<relation::intermediate::limit::sort_key> { c0 },
            row_slice(3),
    });
    auto&& r4 = r.insert(relation::emit { c0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r2.left());
}

TEST_F(push_down_limits_test, left_outer_join_max_partial_limit_count) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 - limit:r3[c0](3) - emit:r4
     * => partial limit is placed only if 3 <= max_partial_limit_count, and max_partial_limit_count != 0
     */
    struct param {
        std::size_t max;
        bool placed;
    };
    for (auto&& [max, placed] : {
            param { 2, false },
            param { 3, true },
            param { 0, false },
    }) {
        max_partial_limit_count = max;
        relation::graph_type r;
        auto&& r0 = add_scan(r);
        auto&& r1 = add_scan1(r);
        auto&& r2 = r.insert(relation::intermediate::join {
                relation::join_kind::left_outer,
        });
        auto&& r3 = r.insert(limit({ c0 }));
        auto&& r4 = r.insert(relation::emit { c0 });
        r0.output() >> r2.left();
        r1.output() >> r2.right();
        r2.output() >> r3.input();
        r3.output() >> r4.input();

        apply(r);

        EXPECT_EQ(static_cast<bool>(upstream_limit(r2.left())), placed) << max;
        EXPECT_GT(r2.output(), r3.input()) << max;
    }
}

TEST_F(push_down_limits_test, left_outer_join_partial_over_project) {
    /*
     * scan:r0 - project:r1 -\
     * scan:r2 -------------- join[left_outer]:r3 - limit:r4[c0](3) - emit:r5
     * =>
     * scan:r0 - limit:p[c0](3) - project:r1 -\
     * scan:r2 ------------------------------- join[left_outer]:r3 - limit:r4[c0](3) - emit:r5
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto&& r2 = add_scan1(r);
    auto&& r3 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r4 = r.insert(limit({ c0 }));
    auto&& r5 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> r4.input();
    r4.output() >> r5.input();

    apply(r);

    ASSERT_EQ(r.size(), 7);
    EXPECT_GT(r1.output(), r3.left());
    auto p = upstream_limit(r1.input());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, left_outer_join_partial_over_apply) {
    /*
     * scan:r0 - apply:r1 -\
     * scan:r2 ------------ join[left_outer]:r3 - limit:r4[c0](3) - emit:r5
     * =>
     * scan:r0 - apply:r1 - limit:p[c0](3) -\
     * scan:r2 ----------------------------- join[left_outer]:r3 - limit:r4[c0](3) - emit:r5
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
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
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    varref { c1 },
            },
            {
                    { 0, x0 },
            },
    });
    auto&& r2 = add_scan1(r);
    auto&& r3 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r4 = r.insert(limit({ c0 }));
    auto&& r5 = r.insert(relation::emit { c0, x0, d0 });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> r4.input();
    r4.output() >> r5.input();

    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto p = upstream_limit(r3.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r1.output(), p->input());
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_EQ(p->row_slice(), row_slice(3));
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, outer_apply) {
    /*
     * scan:r0 - apply[outer]:r1 - limit:r2[c0](3) - emit:r3
     * =>
     * scan:r0 - limit:p[c0](3) - apply[outer]:r1 - limit:r2[c0](3) - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = add_apply(r, relation::apply_kind::outer, x0);
    auto&& r2 = r.insert(limit({ c0 }));
    auto&& r3 = r.insert(relation::emit { c0, x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto p = upstream_limit(r1.input());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->group_keys().size(), 0);
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_EQ(p->row_slice(), row_slice(3));
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, outer_apply_without_sort_keys) {
    /*
     * scan:r0 - apply[outer]:r1 - limit:r2(3) - emit:r3
     * =>
     * scan:r0 - limit:p(3) - apply[outer]:r1 - limit:r2(3) - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = add_apply(r, relation::apply_kind::outer, x0);
    auto&& r2 = r.insert(limit());
    auto&& r3 = r.insert(relation::emit { c0, x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto p = upstream_limit(r1.input());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->sort_keys().size(), 0);
    EXPECT_EQ(p->row_slice(), row_slice(3));
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(push_down_limits_test, outer_apply_sort_key_defined) {
    /*
     * scan:r0 - apply[outer]:r1 - limit:r2[x0](3) - emit:r3
     * => (not changed, because the sort key is defined in the apply)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = add_apply(r, relation::apply_kind::outer, x0);
    auto&& r2 = r.insert(limit({ x0 }));
    auto&& r3 = r.insert(relation::emit { c0, x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, cross_apply) {
    /*
     * scan:r0 - apply[cross]:r1 - limit:r2[c0](3) - emit:r3
     * => (not changed, because cross apply may drop the input rows)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = add_apply(r, relation::apply_kind::cross, x0);
    auto&& r2 = r.insert(limit({ c0 }));
    auto&& r3 = r.insert(relation::emit { c0, x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, left_outer_join_partial_merge) {
    /*
     * scan:r0 - limit:r1[c0](5) -\
     * scan:r2 ------------------- join[left_outer]:r3 - limit:r4[c0](3) - emit:r5
     * =>
     * scan:r0 - limit:r1[c0](3) -\
     * scan:r2 ------------------- join[left_outer]:r3 - limit:r4[c0](3) - emit:r5
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(5, { c0 }));
    auto&& r2 = add_scan1(r);
    auto&& r3 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r4 = r.insert(limit({ c0 }));
    auto&& r5 = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> r4.input();
    r4.output() >> r5.input();

    apply(r);

    ASSERT_EQ(r.size(), 6);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r3.left());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, left_outer_join_partial_merge_over_max_partial_limit_count) {
    /*
     * scan:r0 - limit:r1[c0](5) -     * scan:r2 ------------------- join[left_outer]:r3 - limit:r4[c0](3) - emit:r5
     * => the upstream limit is shrunk even if 3 > max_partial_limit_count, but not if max_partial_limit_count == 0
     */
    struct param {
        std::size_t max;
        std::size_t count;
    };
    for (auto&& [max, count] : {
            param { 2, 3 },
            param { 0, 5 },
    }) {
        max_partial_limit_count = max;
        relation::graph_type r;
        auto&& r0 = add_scan(r);
        auto&& r1 = r.insert(limit(5, { c0 }));
        auto&& r2 = add_scan1(r);
        auto&& r3 = r.insert(relation::intermediate::join {
                relation::join_kind::left_outer,
        });
        auto&& r4 = r.insert(limit({ c0 }));
        auto&& r5 = r.insert(relation::emit { c0 });
        r0.output() >> r1.input();
        r1.output() >> r3.left();
        r2.output() >> r3.right();
        r3.output() >> r4.input();
        r4.output() >> r5.input();

        apply(r);

        ASSERT_EQ(r.size(), 6) << max;
        EXPECT_GT(r1.output(), r3.left()) << max;
        EXPECT_EQ(r1.row_slice(), row_slice(count)) << max;
        EXPECT_GT(r3.output(), r4.input()) << max;
    }
}

TEST_F(push_down_limits_test, left_outer_join_chain) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 -\
     * scan:r3 ----------------------- join[left_outer]:r4 - limit:r5[c0](3) - emit:r6
     * =>
     * scan:r0 - limit:p[c0](3) -\
     * scan:r1 ------------------ join[left_outer]:r2 -\
     * scan:r3 ---------------------------------------- join[left_outer]:r4 - limit:r5[c0](3) - emit:r6
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto e0 = bindings.stream_variable("e0");
    auto e1 = bindings.stream_variable("e1");
    auto&& r3 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, e0 },
                    { t1c1, e1 },
            },
    });
    auto&& r4 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& r5 = r.insert(limit({ c0 }));
    auto&& r6 = r.insert(relation::emit { c0, d0, e0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r4.left();
    r3.output() >> r4.right();
    r4.output() >> r5.input();
    r5.output() >> r6.input();

    apply(r);

    ASSERT_EQ(r.size(), 8);
    EXPECT_GT(r2.output(), r4.left());
    auto p = upstream_limit(r2.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_GT(r4.output(), r5.input());
}

TEST_F(push_down_limits_test, union_all) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 - limit:r3[u0, u1 desc](3) - emit:r4
     * =>
     * scan:r0 - limit:p[c0, c1 desc](3) -\
     * scan:r1 - limit:q[d0, d1 desc](3) - union[all]:r2 - limit:r3[u0, u1 desc](3) - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto u1 = bindings.stream_variable("u1");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
                    { c1, d1, u1 },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit({ u0, { u1, relation::sort_direction::descendant } }));
    auto&& r4 = r.insert(relation::emit { u0, u1 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto p = upstream_limit(r2.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->group_keys().size(), 0);
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> {
            c0,
            { c1, relation::sort_direction::descendant },
    }));
    EXPECT_EQ(p->row_slice(), row_slice(3));

    auto q = upstream_limit(r2.right());
    ASSERT_TRUE(q);
    EXPECT_GT(r1.output(), q->input());
    EXPECT_EQ(q->group_keys().size(), 0);
    EXPECT_EQ(q->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> {
            d0,
            { d1, relation::sort_direction::descendant },
    }));
    EXPECT_EQ(q->row_slice(), row_slice(3));

    EXPECT_GT(r2.output(), r3.input());
    EXPECT_EQ(r3.sort_keys(), (std::vector<relation::intermediate::limit::sort_key> {
            u0,
            { u1, relation::sort_direction::descendant },
    }));
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, union_all_without_sort_keys) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 - limit:r3(3) - emit:r4
     * =>
     * scan:r0 - limit:p(3) -\
     * scan:r1 - limit:q(3) - union[all]:r2 - limit:r3(3) - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit());
    auto&& r4 = r.insert(relation::emit { u0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto p = upstream_limit(r2.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->sort_keys().size(), 0);
    EXPECT_EQ(p->row_slice(), row_slice(3));
    auto q = upstream_limit(r2.right());
    ASSERT_TRUE(q);
    EXPECT_GT(r1.output(), q->input());
    EXPECT_EQ(q->sort_keys().size(), 0);
    EXPECT_EQ(q->row_slice(), row_slice(3));
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, union_all_absent_left_column) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 - limit:r3[u0, u1](3) - emit:r4
     * (u1 is absent in the left input)
     * =>
     * scan:r0 ------------------ \
     * scan:r1 - limit:q[d0, d1](3) - union[all]:r2 - limit:r3[u0, u1](3) - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto u1 = bindings.stream_variable("u1");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
                    { {}, d1, u1 },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit({ u0, u1 }));
    auto&& r4 = r.insert(relation::emit { u0, u1 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 6);
    EXPECT_GT(r0.output(), r2.left());
    auto q = upstream_limit(r2.right());
    ASSERT_TRUE(q);
    EXPECT_GT(r1.output(), q->input());
    EXPECT_EQ(q->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { d0, d1 }));
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, union_all_absent_right_column) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 - limit:r3[u0, u1](3) - emit:r4
     * (u1 is absent in the right input)
     * =>
     * scan:r0 - limit:p[c0, c1](3) -\
     * scan:r1 ---------------------- union[all]:r2 - limit:r3[u0, u1](3) - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto u1 = bindings.stream_variable("u1");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
                    { c1, {}, u1 },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit({ u0, u1 }));
    auto&& r4 = r.insert(relation::emit { u0, u1 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto p = upstream_limit(r2.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0, c1 }));
    EXPECT_GT(r1.output(), r2.right());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, union_all_absent_both_columns) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 - limit:r3[u0, u1, u2](3) - emit:r4
     * (u1 is absent in the left input, and u2 is absent in the right input)
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto u1 = bindings.stream_variable("u1");
    auto u2 = bindings.stream_variable("u2");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
                    { {}, d1, u1 },
                    { c1, {}, u2 },
            },
            relation::set_quantifier::all,
    });
    auto&& r3 = r.insert(limit({ u0, u1, u2 }));
    auto&& r4 = r.insert(relation::emit { u0, u1, u2 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r2.left());
    EXPECT_GT(r1.output(), r2.right());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, union_distinct) {
    /*
     * scan:r0 -\
     * scan:r1 - union[distinct]:r2 - limit:r3[u0](3) - emit:r4
     * => (not changed)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
            },
            relation::set_quantifier::distinct,
    });
    auto&& r3 = r.insert(limit({ u0 }));
    auto&& r4 = r.insert(relation::emit { u0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r2.left());
    EXPECT_GT(r1.output(), r2.right());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(push_down_limits_test, union_all_max_partial_limit_count) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 - limit:r3[u0](3) - emit:r4
     * => (not changed if 3 > max_partial_limit_count, or max_partial_limit_count == 0)
     */
    for (std::size_t max : { 2, 0 }) {
        max_partial_limit_count = max;
        relation::graph_type r;
        auto&& r0 = add_scan(r);
        auto&& r1 = add_scan1(r);
        auto u0 = bindings.stream_variable("u0");
        auto&& r2 = r.insert(relation::intermediate::union_ {
                {
                        { c0, d0, u0 },
                },
                relation::set_quantifier::all,
        });
        auto&& r3 = r.insert(limit({ u0 }));
        auto&& r4 = r.insert(relation::emit { u0 });
        r0.output() >> r2.left();
        r1.output() >> r2.right();
        r2.output() >> r3.input();
        r3.output() >> r4.input();

        apply(r);

        ASSERT_EQ(r.size(), 5) << max;
        EXPECT_GT(r0.output(), r2.left()) << max;
        EXPECT_GT(r1.output(), r2.right()) << max;
        EXPECT_GT(r2.output(), r3.input()) << max;
    }
}

TEST_F(push_down_limits_test, union_all_nested) {
    /*
     * scan:r0 -\
     * scan:r1 - union[all]:r2 -\
     * scan:r3 ----------------- union[all]:r4 - limit:r5[w0](3) - emit:r6
     * =>
     * scan:r0 - limit:p0[c0](3) -\
     * scan:r1 - limit:p1[d0](3) - union[all]:r2 -\
     * scan:r3 - limit:p2[e0](3) ----------------- union[all]:r4 - limit:r5[w0](3) - emit:r6
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto&& r2 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
            },
            relation::set_quantifier::all,
    });
    auto e0 = bindings.stream_variable("e0");
    auto e1 = bindings.stream_variable("e1");
    auto&& r3 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, e0 },
                    { t1c1, e1 },
            },
    });
    auto w0 = bindings.stream_variable("w0");
    auto&& r4 = r.insert(relation::intermediate::union_ {
            {
                    { u0, e0, w0 },
            },
            relation::set_quantifier::all,
    });
    auto&& r5 = r.insert(limit({ w0 }));
    auto&& r6 = r.insert(relation::emit { w0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r4.left();
    r3.output() >> r4.right();
    r4.output() >> r5.input();
    r5.output() >> r6.input();

    apply(r);

    ASSERT_EQ(r.size(), 10);
    auto p0 = upstream_limit(r2.left());
    ASSERT_TRUE(p0);
    EXPECT_GT(r0.output(), p0->input());
    EXPECT_EQ(p0->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_EQ(p0->row_slice(), row_slice(3));

    auto p1 = upstream_limit(r2.right());
    ASSERT_TRUE(p1);
    EXPECT_GT(r1.output(), p1->input());
    EXPECT_EQ(p1->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { d0 }));
    EXPECT_EQ(p1->row_slice(), row_slice(3));

    auto p2 = upstream_limit(r4.right());
    ASSERT_TRUE(p2);
    EXPECT_GT(r3.output(), p2->input());
    EXPECT_EQ(p2->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { e0 }));
    EXPECT_EQ(p2->row_slice(), row_slice(3));

    EXPECT_GT(r2.output(), r4.left());
    EXPECT_GT(r4.output(), r5.input());
}

TEST_F(push_down_limits_test, union_all_left_outer_join) {
    /*
     * scan:r0 -\
     * scan:r1 - join[left_outer]:r2 -\
     * scan:r3 ----------------------- union[all]:r4 - limit:r5[w0](3) - emit:r6
     * =>
     * scan:r0 - limit:p[c0](3) -\
     * scan:r1 ------------------ join[left_outer]:r2 -\
     * scan:r3 - limit:q[e0](3) ------------------------ union[all]:r4 - limit:r5[w0](3) - emit:r6
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan1(r);
    auto&& r2 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto e0 = bindings.stream_variable("e0");
    auto e1 = bindings.stream_variable("e1");
    auto&& r3 = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, e0 },
                    { t1c1, e1 },
            },
    });
    auto w0 = bindings.stream_variable("w0");
    auto&& r4 = r.insert(relation::intermediate::union_ {
            {
                    { c0, e0, w0 },
            },
            relation::set_quantifier::all,
    });
    auto&& r5 = r.insert(limit({ w0 }));
    auto&& r6 = r.insert(relation::emit { w0 });
    r0.output() >> r2.left();
    r1.output() >> r2.right();
    r2.output() >> r4.left();
    r3.output() >> r4.right();
    r4.output() >> r5.input();
    r5.output() >> r6.input();

    apply(r);

    ASSERT_EQ(r.size(), 9);
    auto p = upstream_limit(r2.left());
    ASSERT_TRUE(p);
    EXPECT_GT(r0.output(), p->input());
    EXPECT_EQ(p->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_GT(r1.output(), r2.right());
    EXPECT_GT(r2.output(), r4.left());

    auto q = upstream_limit(r4.right());
    ASSERT_TRUE(q);
    EXPECT_GT(r3.output(), q->input());
    EXPECT_EQ(q->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { e0 }));

    EXPECT_GT(r4.output(), r5.input());
}

TEST_F(push_down_limits_test, union_all_partial_merge) {
    /*
     * scan:r0 - limit:r1[c0](5) -\
     * scan:r2 ------------------- union[all]:r3 - limit:r4[u0](3) - emit:r5
     * =>
     * scan:r0 - limit:r1[c0](3) -\
     * scan:r2 - limit:q[d0](3) -- union[all]:r3 - limit:r4[u0](3) - emit:r5
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(5, { c0 }));
    auto&& r2 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto&& r3 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
            },
            relation::set_quantifier::all,
    });
    auto&& r4 = r.insert(limit({ u0 }));
    auto&& r5 = r.insert(relation::emit { u0 });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> r4.input();
    r4.output() >> r5.input();

    apply(r);

    ASSERT_EQ(r.size(), 7);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r3.left());
    EXPECT_EQ(r1.sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { c0 }));
    EXPECT_EQ(r1.row_slice(), row_slice(3));

    auto q = upstream_limit(r3.right());
    ASSERT_TRUE(q);
    EXPECT_GT(r2.output(), q->input());
    EXPECT_EQ(q->sort_keys(), (std::vector<relation::intermediate::limit::sort_key> { d0 }));
    EXPECT_EQ(q->row_slice(), row_slice(3));

    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, union_all_partial_merge_over_max_partial_limit_count) {
    /*
     * scan:r0 - limit:r1[c0](5) -\
     * scan:r2 ------------------- union[all]:r3 - limit:r4[u0](3) - emit:r5
     * (3 > max_partial_limit_count)
     * =>
     * scan:r0 - limit:r1[c0](3) -\
     * scan:r2 ------------------- union[all]:r3 - limit:r4[u0](3) - emit:r5
     */
    max_partial_limit_count = 2;
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(limit(5, { c0 }));
    auto&& r2 = add_scan1(r);
    auto u0 = bindings.stream_variable("u0");
    auto&& r3 = r.insert(relation::intermediate::union_ {
            {
                    { c0, d0, u0 },
            },
            relation::set_quantifier::all,
    });
    auto&& r4 = r.insert(limit({ u0 }));
    auto&& r5 = r.insert(relation::emit { u0 });
    r0.output() >> r1.input();
    r1.output() >> r3.left();
    r2.output() >> r3.right();
    r3.output() >> r4.input();
    r4.output() >> r5.input();

    apply(r);

    ASSERT_EQ(r.size(), 6);
    EXPECT_GT(r1.output(), r3.left());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    EXPECT_GT(r2.output(), r3.right());
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, fence) {
    /*
     * scan:r0 - project:r1 - fence:r2 - limit:r3 - emit:r4
     * =>
     * (no changes: limit must not go over the fence)
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto&& r2 = r.insert(extension::relation::fence { 7, 0 });
    auto&& r3 = r.insert(limit());
    auto&& r4 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(push_down_limits_test, fence_downstream) {
    /*
     * scan:r0 - fence:r1 - project:r2 - limit:r3 - emit:r4
     * =>
     * scan:r0 - fence:r1 - limit:r3 - project:r2 - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(extension::relation::fence { 7, 0 });
    auto x0 = bindings.stream_variable("x0");
    auto&& r2 = r.insert(relation::project {
            relation::project::column { x0, plus(c0, 1) },
    });
    auto&& r3 = r.insert(limit());
    auto&& r4 = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_GT(r3.output(), r2.input());
    EXPECT_GT(r2.output(), r4.input());
}

} // namespace yugawara::analyzer::details
