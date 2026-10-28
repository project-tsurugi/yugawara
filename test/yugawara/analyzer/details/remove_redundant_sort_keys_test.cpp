#include <yugawara/analyzer/details/remove_redundant_sort_keys.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/limit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::relation::sort_direction;

class remove_redundant_sort_keys_test : public ::testing::Test {
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

    /*
     * scan:r0 - limit:rl - emit:r1
     */
    relation::intermediate::limit& build(
            relation::graph_type& r,
            std::vector<relation::intermediate::limit::sort_key> sort_keys) {
        auto&& r0 = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                        { t0c2, c2 },
                },
        });
        auto&& rl = r.insert(relation::intermediate::limit {
                {},
                std::move(sort_keys),
                relation::intermediate::limit::row_slice_type { {}, 10 },
        });
        auto&& r1 = r.insert(relation::emit { c0, c1, c2 });
        r0.output() >> rl.input();
        rl.output() >> r1.input();
        return rl;
    }
};

TEST_F(remove_redundant_sort_keys_test, stay) {
    relation::graph_type r;
    auto&& rl = build(r, {
            { c1, sort_direction::descendant },
            { c0 },
            { c2 },
    });

    remove_redundant_sort_keys(r);

    ASSERT_EQ(rl.sort_keys().size(), 3);
    EXPECT_EQ(rl.sort_keys()[0], (relation::intermediate::limit::sort_key { c1, sort_direction::descendant }));
    EXPECT_EQ(rl.sort_keys()[1], (relation::intermediate::limit::sort_key { c0 }));
    EXPECT_EQ(rl.sort_keys()[2], (relation::intermediate::limit::sort_key { c2 }));
}

TEST_F(remove_redundant_sort_keys_test, same_direction) {
    relation::graph_type r;
    auto&& rl = build(r, {
            { c0 },
            { c1 },
            { c0 },
    });

    remove_redundant_sort_keys(r);

    ASSERT_EQ(rl.sort_keys().size(), 2);
    EXPECT_EQ(rl.sort_keys()[0], (relation::intermediate::limit::sort_key { c0 }));
    EXPECT_EQ(rl.sort_keys()[1], (relation::intermediate::limit::sort_key { c1 }));
}

TEST_F(remove_redundant_sort_keys_test, different_direction) {
    relation::graph_type r;
    auto&& rl = build(r, {
            { c0 },
            { c1 },
            { c0, sort_direction::descendant },
    });

    remove_redundant_sort_keys(r);

    ASSERT_EQ(rl.sort_keys().size(), 2);
    EXPECT_EQ(rl.sort_keys()[0], (relation::intermediate::limit::sort_key { c0 }));
    EXPECT_EQ(rl.sort_keys()[1], (relation::intermediate::limit::sort_key { c1 }));
}

TEST_F(remove_redundant_sort_keys_test, multiple) {
    relation::graph_type r;
    auto&& rl = build(r, {
            { c1, sort_direction::descendant },
            { c1 },
            { c0 },
            { c1 },
            { c2 },
            { c0, sort_direction::descendant },
    });

    remove_redundant_sort_keys(r);

    ASSERT_EQ(rl.sort_keys().size(), 3);
    EXPECT_EQ(rl.sort_keys()[0], (relation::intermediate::limit::sort_key { c1, sort_direction::descendant }));
    EXPECT_EQ(rl.sort_keys()[1], (relation::intermediate::limit::sort_key { c0 }));
    EXPECT_EQ(rl.sort_keys()[2], (relation::intermediate::limit::sort_key { c2 }));
    EXPECT_EQ(rl.row_slice(), (relation::intermediate::limit::row_slice_type { {}, 10 }));
}

} // namespace yugawara::analyzer::details
