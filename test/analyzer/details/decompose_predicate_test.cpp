#include <analyzer/details/decompose_projections.h>

#include <gtest/gtest.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/emit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include "utils.h"

namespace yugawara::analyzer::details {

class decompose_predicate_test : public ::testing::Test {

protected:
    ::takatori::util::object_creator creator;
    binding::factory bindings;
    storage::configurable_provider storages;

    std::shared_ptr<storage::relation> t0 = storages.add_relation("T0", storage::table {
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    descriptor::variable t0c0 = bindings(t0->columns()[0]);
    descriptor::variable t0c1 = bindings(t0->columns()[0]);
    descriptor::variable t0c2 = bindings(t0->columns()[0]);

    std::shared_ptr<storage::index> i0 = storages.add_index("I0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "I0",
    });

    void apply(relation::graph_type& graph) {
        details::decompose_projections(graph, creator);
    }
};

TEST_F(decompose_predicate_test, simple) {
    /*
     * scan:r0 - project:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::project {
            relation::project::column { varref { c0 }, x0 },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r1));
    auto&& p0 = r1;

    EXPECT_GT(p0.output(), ro.input());

    ASSERT_EQ(p0.columns().size(), 1);
    EXPECT_EQ(p0.columns()[0].variable(), x0);
    EXPECT_EQ(p0.columns()[0].value(), varref(c0));
}

TEST_F(decompose_predicate_test, decompose) {
    /*
     * scan:r0 - project:r2 - emit:ro
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto& r1 = r.insert(relation::project {
            relation::project::column { varref { c0 }, x0 },
            relation::project::column { constant(1), x1 },
            relation::project::column { constant(2), x2 },
    });
    auto& ro = r.insert(relation::emit {
            x0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 5);

    auto&& p0 = r1;
    ASSERT_TRUE(r.contains(p0));

    auto&& p1 = next<relation::project>(p0.output());
    ASSERT_TRUE(r.contains(p1));

    auto&& p2 = next<relation::project>(p1.output());
    ASSERT_TRUE(r.contains(p2));

    EXPECT_GT(p2.output(), ro.input());

    ASSERT_EQ(p0.columns().size(), 1);
    EXPECT_EQ(p0.columns()[0].variable(), x0);
    EXPECT_EQ(p0.columns()[0].value(), varref(c0));

    ASSERT_EQ(p1.columns().size(), 1);
    EXPECT_EQ(p1.columns()[0].variable(), x1);
    EXPECT_EQ(p1.columns()[0].value(), constant(1));

    ASSERT_EQ(p2.columns().size(), 1);
    EXPECT_EQ(p2.columns()[0].variable(), x2);
    EXPECT_EQ(p2.columns()[0].value(), constant(2));
}

} // namespace yugawara::analyzer::details
