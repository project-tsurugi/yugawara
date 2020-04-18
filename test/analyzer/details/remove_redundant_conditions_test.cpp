#include <analyzer/details/remove_redundant_conditions.h>

#include <gtest/gtest.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/emit.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include "utils.h"

namespace yugawara::analyzer::details {

class remove_redundant_conditions_test : public ::testing::Test {
protected:
    ::takatori::util::object_creator creator;
    type::repository types;
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
    std::shared_ptr<storage::relation> t1 = storages.add_relation("T1", storage::table {
            "T1",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    descriptor::variable t0c0 = bindings(t0->columns()[0]);
    descriptor::variable t0c1 = bindings(t0->columns()[1]);
    descriptor::variable t0c2 = bindings(t0->columns()[2]);
    descriptor::variable t1c0 = bindings(t1->columns()[0]);
    descriptor::variable t1c1 = bindings(t1->columns()[1]);
    descriptor::variable t1c2 = bindings(t1->columns()[2]);

    std::shared_ptr<storage::index> i0 = storages.add_index("I0", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t0),
            "I0",
            {
                    t0->columns()[0],
            },
            {
                    t0->columns()[1],
                    t0->columns()[2],
            }
    });
    std::shared_ptr<storage::index> i1 = storages.add_index("I1", storage::index {
            std::dynamic_pointer_cast<storage::table const>(t1),
            "I1",
            {
                    t1->columns()[0],
            },
            {
                    t1->columns()[1],
                    t1->columns()[2],
            }
    });

    void apply(relation::graph_type& r) {
        remove_redundant_conditions(r);
    }
};

TEST_F(remove_redundant_conditions_test, filter_true) {
    /*
     * scan:r0 - filter:rf - emit:r1
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            boolean(true)
    });
    auto&& r1 = r.insert(relation::emit {
            c0
    });
    r0.output() >> rf.input();
    rf.output() >> r1.input();

    apply(r);

    ASSERT_EQ(r.size(), 2);
    EXPECT_GT(r0.output(), r1.input());
}

TEST_F(remove_redundant_conditions_test, filter_stay) {
    /*
     * scan:r0 - filter:rf - emit:r1
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(1))
    });
    auto&& r1 = r.insert(relation::emit {
            c0,
    });
    r0.output() >> rf.input();
    rf.output() >> r1.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r0.output(), rf.input());
    EXPECT_GT(rf.output(), r1.input());
    EXPECT_EQ(rf.condition(), compare(varref(c0), constant(1)));
}

TEST_F(remove_redundant_conditions_test, filter_shrink) {
    /*
     * scan:r0 - filter:rf - emit:r1
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    boolean(true),
                    compare(varref(c0), constant(1)))
    });
    auto&& r1 = r.insert(relation::emit {
            c0
    });
    r0.output() >> rf.input();
    rf.output() >> r1.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    EXPECT_GT(r0.output(), rf.input());
    EXPECT_GT(rf.output(), r1.input());
    EXPECT_EQ(rf.condition(), compare(varref(c0), constant(1)));
}

TEST_F(remove_redundant_conditions_test, join_relation_true) {
    /*
     * scan:rl -\
     *           join_relation:r0 - emit:ro
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cr0 = bindings.stream_variable("cr0");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
            },
    });
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            boolean(true),
    });
    auto&& ro = r.insert(relation::emit {
            cl0,
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_EQ(r0.condition(), nullptr);
}

TEST_F(remove_redundant_conditions_test, join_relation_stay) {
    /*
     * scan:rl -\
     *           join_relation:r0 - emit:ro
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cr0 = bindings.stream_variable("cr0");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
            },
    });
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(cl0, cr0),
    });
    auto&& ro = r.insert(relation::emit {
            cl0,
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_EQ(r0.condition(), compare(cl0, cr0));
}

TEST_F(remove_redundant_conditions_test, join_relation_shrink) {
    /*
     * scan:rl -\
     *           join_relation:r0 - emit:ro
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cr0 = bindings.stream_variable("cr0");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
            },
    });
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            land(compare(cl0, cr0), boolean(true)),
    });
    auto&& ro = r.insert(relation::emit {
            cl0,
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_EQ(r0.condition(), compare(cl0, cr0));
}

} // namespace yugawara::analyzer::details
