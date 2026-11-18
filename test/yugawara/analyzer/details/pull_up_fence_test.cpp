#include <yugawara/analyzer/details/pull_up_fence.h>

#include <memory>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/scalar/function_call.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/relation/fence.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/function/declaration.h>

#include <yugawara/testing/utils.h>
#include <yugawara/testing/mock_symbol_location_query.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class pull_up_fence_test : public ::testing::Test {
protected:
    binding::factory bindings;
    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
            },
    });
    descriptor::variable t0c0 = bindings(t0->columns()[0]);
    descriptor::variable t0c1 = bindings(t0->columns()[1]);
    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });

    std::shared_ptr<function::declaration> local_function = std::make_shared<function::declaration>(
            function::declaration {
                    function::declaration::minimum_builtin_function_id + 1,
                    "local_function",
                    ::takatori::type::boolean {},
                    {},
            });

    mock_symbol_location_query query {};
    remote::location_options location_options = location_options_of(query);

    descriptor::variable c0 = bindings.stream_variable("c0");
    descriptor::variable c1 = bindings.stream_variable("c1");
    descriptor::variable x0 = bindings.stream_variable("x0");
    descriptor::variable x1 = bindings.stream_variable("x1");

    // available locations: { 0, 7 }, local_function: { 0 }
    void apply(relation::graph_type& r) {
        query.all = { 0, 7 };
        query.functions.emplace(local_function.get(), remote::symbol_location_query::location_id_set_type { 0 });
        pull_up_fence(r, remote::details::element_location { location_options });
    }

    relation::scan& scan(relation::graph_type& r) {
        return r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                },
        });
    }

    relation::project& project(relation::graph_type& r, descriptor::variable const& variable) {
        return r.insert(relation::project {
                relation::project::column {
                        varref(c0),
                        variable,
                },
        });
    }

    relation::filter& remote_filter(relation::graph_type& r, descriptor::variable const& variable) {
        return r.insert(relation::filter {
                compare(varref(variable), constant(0)),
        });
    }

    relation::filter& local_filter(relation::graph_type& r) {
        return r.insert(relation::filter {
                scalar::function_call { bindings(local_function), {} },
        });
    }
};

TEST_F(pull_up_fence_test, project_filter) {
    /*
     * scan:r0 - fence:rf - project:r1 - filter:r2 - emit:r3
     * =>
     * scan:r0 - project:r1 - filter:r2 - fence:rf - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& r2 = remote_filter(r, x0);
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), rf.input());
    EXPECT_GT(rf.output(), r3.input());
}

TEST_F(pull_up_fence_test, filter_only) {
    /*
     * scan:r0 - fence:rf - filter:r1 - emit:r2
     * =>
     * scan:r0 - filter:r1 - fence:rf - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = remote_filter(r, c0);
    auto&& r2 = r.insert(relation::emit { c0 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), rf.input());
    EXPECT_GT(rf.output(), r2.input());
}

TEST_F(pull_up_fence_test, keep_trailing_project) {
    /*
     * scan:r0 - fence:rf - project:r1 - filter:r2 - project:r3 - emit:r4
     * =>
     * scan:r0 - project:r1 - filter:r2 - fence:rf - project:r3 - emit:r4
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& r2 = remote_filter(r, x0);
    auto&& r3 = project(r, x1);
    auto&& r4 = r.insert(relation::emit { x1 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();

    apply(r);

    ASSERT_EQ(r.size(), 6);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), rf.input());
    EXPECT_GT(rf.output(), r3.input());
    EXPECT_GT(r3.output(), r4.input());
}

TEST_F(pull_up_fence_test, local_project) {
    /*
     * scan:r0 - fence:rf - project:r1 - filter:r2 - emit:r3
     * (r1 has a column only available in the local location)
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    scalar::function_call { bindings(local_function), {} },
                    x0,
            },
    });
    auto&& r2 = remote_filter(r, c0);
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    // the fence stays, because the project is barrier
    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), rf.input());
    EXPECT_GT(rf.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(pull_up_fence_test, local_filter) {
    /*
     * scan:r0 - fence:rf - project:r1 - filter:r2 - filter:r3 - filter:r4 - emit:r5
     * =>
     * scan:r0 - project:r1 - filter:r2 - fence:rf - filter:r3 - filter:r4 - emit:r5
     * (r3 is only available in the local location)
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& r2 = remote_filter(r, x0);
    auto&& r3 = local_filter(r);
    auto&& r4 = remote_filter(r, c1);
    auto&& r5 = r.insert(relation::emit { x0 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();
    r3.output() >> r4.input();
    r4.output() >> r5.input();

    apply(r);

    // the fence stops before r3, and does not take in r4
    ASSERT_EQ(r.size(), 7);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), rf.input());
    EXPECT_GT(rf.output(), r3.input());
    EXPECT_GT(r3.output(), r4.input());
    EXPECT_GT(r4.output(), r5.input());
}

TEST_F(pull_up_fence_test, no_filter) {
    /*
     * scan:r0 - fence:rf - project:r1 - emit:r2
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& r2 = r.insert(relation::emit { x0 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();

    apply(r);

    // the fence stays, because there are no operators of gain
    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), rf.input());
    EXPECT_GT(rf.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
}

TEST_F(pull_up_fence_test, other_kind) {
    /*
     * scan:r0 - fence:rf - limit:r1 - filter:r2 - emit:r3
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = r.insert(relation::intermediate::limit {});
    auto&& r2 = remote_filter(r, c0);
    auto&& r3 = r.insert(relation::emit { c0 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    // the fence stays, because limit is barrier even if it can be executed in the upstream location
    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), rf.input());
    EXPECT_GT(rf.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(pull_up_fence_test, filter_kind_not_remote) {
    /*
     * scan:r0 - fence:rf - project:r1 - filter:r2 - emit:r3
     * (filter is not in the remote relation expression kinds)
     */
    location_options.remote_relation_expression_kinds().erase(relation::expression_kind::filter);
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& r2 = remote_filter(r, x0);
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> rf.input();
    rf.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    // the fence stays, because the filter cannot be executed in the upstream location
    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), rf.input());
    EXPECT_GT(rf.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), r3.input());
}

TEST_F(pull_up_fence_test, write_upstream_local) {
    /*
     * scan:r0 - project:r1 - fence:rf - write:r2
     * (the upstream of the fence is local)
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& r1 = project(r, x0);
    auto&& rf = r.insert(extension::relation::fence { 0, 7 });
    auto&& r2 = r.insert(relation::write {
            relation::write_kind::insert,
            bindings(*i0),
            {},
            {
                    { x0, t0c0, },
            },
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();
    rf.output() >> r2.input();

    apply(r);

    // the fence stays, because write is barrier
    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), rf.input());
    EXPECT_GT(rf.output(), r2.input());
}

TEST_F(pull_up_fence_test, other_fence) {
    /*
     * scan:r0 - fence:rf0 - project:r1 - fence:rf1 - filter:r2 - emit:r3
     * =>
     * scan:r0 - project:r1 - filter:r2 - fence:rf0{7, 8} - emit:r3
     * (rf1 moves first, and then rf0 follows it, and takes in rf1)
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf0 = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& rf1 = r.insert(extension::relation::fence { 0, 8 });
    auto&& r2 = remote_filter(r, x0);
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> rf0.input();
    rf0.output() >> r1.input();
    r1.output() >> rf1.input();
    rf1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), rf0.input());
    EXPECT_GT(rf0.output(), r3.input());
    EXPECT_EQ(rf0.upstream(), 7);
    EXPECT_EQ(rf0.downstream(), 8);
}

TEST_F(pull_up_fence_test, take_in_adjacent_fence) {
    /*
     * scan:r0 - fence:rf0 - fence:rf1 - emit:r1
     * =>
     * scan:r0 - fence:rf0{7, 8} - emit:r1
     * (rf0 takes in the adjacent fence)
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf0 = r.insert(extension::relation::fence { 7, 0 });
    auto&& rf1 = r.insert(extension::relation::fence { 0, 8 });
    auto&& r1 = r.insert(relation::emit { c0 });
    r0.output() >> rf0.input();
    rf0.output() >> rf1.input();
    rf1.output() >> r1.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r0.output(), rf0.input());
    EXPECT_GT(rf0.output(), r1.input());
    EXPECT_EQ(rf0.upstream(), 7);
    EXPECT_EQ(rf0.downstream(), 8);
}

TEST_F(pull_up_fence_test, take_in_fence_same_location) {
    /*
     * scan:r0 - fence:rf0 - project:r1 - filter:r2 - fence:rf1 - write:r3
     * =>
     * scan:r0 - project:r1 - filter:r2 - fence:rf0{7, 7} - write:r3
     * (the fence remains even if its upstream and downstream locations become the same)
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf0 = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& r2 = remote_filter(r, x0);
    auto&& rf1 = r.insert(extension::relation::fence { 0, 7 });
    auto&& r3 = r.insert(relation::write {
            relation::write_kind::insert,
            bindings(*i0),
            {},
            {
                    { x0, t0c0, },
            },
    });
    r0.output() >> rf0.input();
    rf0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> rf1.input();
    rf1.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), rf0.input());
    EXPECT_GT(rf0.output(), r3.input());
    EXPECT_EQ(rf0.upstream(), 7);
    EXPECT_EQ(rf0.downstream(), 7);
}

TEST_F(pull_up_fence_test, take_in_project_and_fence) {
    /*
     * scan:r0 - fence:rf0 - filter:r1 - project:r2 - fence:rf1 - emit:r3
     * =>
     * scan:r0 - filter:r1 - project:r2 - fence:rf0{7, 8} - emit:r3
     * (rf0 takes in the project and the following fence, because the fence is gain)
     */
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf0 = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = remote_filter(r, c0);
    auto&& r2 = project(r, x0);
    auto&& rf1 = r.insert(extension::relation::fence { 0, 8 });
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> rf0.input();
    rf0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> rf1.input();
    rf1.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), rf0.input());
    EXPECT_GT(rf0.output(), r3.input());
    EXPECT_EQ(rf0.upstream(), 7);
    EXPECT_EQ(rf0.downstream(), 8);
}

TEST_F(pull_up_fence_test, move_with_following_fence) {
    /*
     * scan:r0 - fence:rf0{7, 0} - project:r1 - fence:rf1{0, 8} - filter:r2 - emit:r3
     * =>
     * scan:r0 - project:r1 - filter:r2 - fence:rf0{7, 8} - emit:r3
     * (r2 is only available in the remote location 7, so that rf1 cannot move by itself,
     *  but rf0 takes in rf1 and moves downstream beyond it)
     */
    auto remote_function = std::make_shared<function::declaration>(
            function::declaration {
                    function::declaration::minimum_builtin_function_id + 2,
                    "remote_function",
                    ::takatori::type::boolean {},
                    {},
            });
    query.functions.emplace(remote_function.get(), remote::symbol_location_query::location_id_set_type { 7 });
    relation::graph_type r;
    auto&& r0 = scan(r);
    auto&& rf0 = r.insert(extension::relation::fence { 7, 0 });
    auto&& r1 = project(r, x0);
    auto&& rf1 = r.insert(extension::relation::fence { 0, 8 });
    auto&& r2 = r.insert(relation::filter {
            scalar::function_call { bindings(remote_function), {} },
    });
    auto&& r3 = r.insert(relation::emit { x0 });
    r0.output() >> rf0.input();
    rf0.output() >> r1.input();
    r1.output() >> rf1.input();
    rf1.output() >> r2.input();
    r2.output() >> r3.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_GT(r1.output(), r2.input());
    EXPECT_GT(r2.output(), rf0.input());
    EXPECT_GT(rf0.output(), r3.input());
    EXPECT_EQ(rf0.upstream(), 7);
    EXPECT_EQ(rf0.downstream(), 8);
}

} // namespace yugawara::analyzer::details
