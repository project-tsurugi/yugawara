#include <yugawara/analyzer/details/push_down_limits.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/scalar/binary.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>

#include <yugawara/binding/factory.h>
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

    static void apply(relation::graph_type& r) {
        push_down_limits(r);
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
     * scan:r0 - limit:r1[c0](3) - emit:r3
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

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    ASSERT_EQ(r1.sort_keys().size(), 1);
    EXPECT_EQ(r1.sort_keys()[0].variable(), c0);
}

TEST_F(push_down_limits_test, merge_prefix) {
    /*
     * scan:r0 - limit:r1[c0, c1](3) - limit:r2[c0](10) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0, c1](3) - emit:r3
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

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    ASSERT_EQ(r1.sort_keys().size(), 2);
}

TEST_F(push_down_limits_test, merge_without_sort_keys) {
    /*
     * scan:r0 - limit:r1[c0](10) - limit:r2(3) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0](3) - emit:r3
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

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
    ASSERT_EQ(r1.sort_keys().size(), 1);
}

TEST_F(push_down_limits_test, merge_inner_without_count) {
    /*
     * scan:r0 - limit:r1[c0] - limit:r2[c0](3) - emit:r3
     * =>
     * scan:r0 - limit:r1[c0](3) - emit:r3
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

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r3.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
}

TEST_F(push_down_limits_test, merge_over_project) {
    /*
     * scan:r0 - limit:r1[c0](10) - project:r2 - limit:r3[c0](3) - emit:r4
     * =>
     * scan:r0 - limit:r1[c0](3) - project:r2 - emit:r4
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

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r4.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
}

TEST_F(push_down_limits_test, merge_chain) {
    /*
     * scan:r0 - limit:r1[c0](10) - limit:r2[c0](5) - limit:r3(3) - emit:r4
     * =>
     * scan:r0 - limit:r1[c0](3) - emit:r4
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

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r4.input());
    EXPECT_EQ(r1.row_slice(), row_slice(3));
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
     * scan:r0 - limit:r1[c0](n) - emit:r3
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

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r1.output(), r3.input());
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

} // namespace yugawara::analyzer::details
