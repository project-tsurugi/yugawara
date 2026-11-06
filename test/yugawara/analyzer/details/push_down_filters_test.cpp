#include <yugawara/analyzer/details/push_down_filters.h>

#include <algorithm>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/identify.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/write.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/escape.h>

#include <takatori/util/downcast.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::util::downcast;

class push_down_filters_test : public ::testing::Test {
protected:
    type::repository types;
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
    std::shared_ptr<storage::table> t1 = storages.add_table({
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

    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });
    std::shared_ptr<storage::index> i1 = storages.add_index({ t1, "I1" });

    aggregate::configurable_provider aggregates;
    std::shared_ptr<aggregate::declaration> agg0 = aggregates.add(aggregate::declaration {
            aggregate::declaration::minimum_builtin_function_id + 1,
            "agg0",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });
    std::shared_ptr<aggregate::declaration> agg1 = aggregates.add(aggregate::declaration {
            agg0->definition_id() + 1,
            "agg1",
            t::int4 {},
            {
                    t::int4 {},
            },
            true,
    });

    static relation::expression::input_port_type& connect(relation::expression::output_port_type& output) {
        if (output.opposite()) {
            throw std::domain_error("must not be connected");
        }

        auto&& r = output.owner().owner();

        /*
         * .. - emit
         */
        auto&& emit = r.insert(relation::emit {});
        return emit.input() << output;
    }

    void apply(relation::graph_type& r) {
        push_down_selections(r);
    }

    using expression_list = std::vector<scalar::expression const*>;

    /**
     * @brief collects the conjunctive terms of the given expression.
     * @details This ignores the constant `TRUE`, because it never affects the condition
     *      (see remove_redundant_conditions()).
     */
    static void collect_terms(scalar::expression const& expr, expression_list& results) {
        if (expr.kind() == scalar::binary::tag) {
            auto&& binary = ::takatori::util::unsafe_downcast<scalar::binary>(expr);
            if (binary.operator_kind() == scalar::binary_operator::conditional_and) {
                collect_terms(binary.left(), results);
                collect_terms(binary.right(), results);
                return;
            }
        }
        if (expr != boolean(true)) {
            results.emplace_back(std::addressof(expr));
        }
    }

    static expression_list terms_of(relation::intermediate::join const& expr) {
        expression_list results {};
        if (auto condition = expr.condition()) {
            collect_terms(*condition, results);
        }
        return results;
    }

    static expression_list terms_of(relation::filter const& expr) {
        expression_list results {};
        collect_terms(expr.condition(), results);
        return results;
    }

    static bool contains(expression_list const& terms, scalar::expression const& target) {
        return std::any_of(terms.begin(), terms.end(), [&](auto* term) {
            return *term == target;
        });
    }
};

TEST_F(push_down_filters_test, find) {
    /*
     * scan:r0 - filter:rf - ..
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::find {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
            {
                    relation::find::key { t0c0, constant() },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(c0, c1));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, scan) {
    /*
     * scan:r0 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(c0, c1));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, values) {
    /*
     * values:r0 - filter:rf - ..
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::values {
            { c0, c1, c2, },
            {
                    { constant(0), constant(1), constant(2), },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(c0, c1));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_inner_over_left) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_FALSE(rj.condition());
}

TEST_F(push_down_filters_test, join_relation_inner_over_right) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_FALSE(rj.condition());
}

TEST_F(push_down_filters_test, join_relation_inner_over_both) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(constant(0), constant(0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto&& f0 = next<relation::filter>(rl.output());
    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_GT(f1.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(constant(0), constant(0)));
    EXPECT_EQ(f1.condition(), compare(constant(0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_FALSE(rj.condition());
}

TEST_F(push_down_filters_test, join_relation_inner_to_condition) {
    /*
     * scan:rl -\
     *           join_relation:rj - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), varref(cr0)),
    });
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);

    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr0)));
}

TEST_F(push_down_filters_test, join_relation_inner_to_left) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(cl0), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));
    EXPECT_TRUE(terms_of(rj).empty());
}

TEST_F(push_down_filters_test, join_relation_inner_to_right) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(cr0), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));
    EXPECT_TRUE(terms_of(rj).empty());
}

TEST_F(push_down_filters_test, join_relation_inner_to_both) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(constant(0), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_GT(f1.output(), rj.right());

    EXPECT_EQ(f0.condition(), compare(constant(0), constant(1)));
    EXPECT_EQ(f1.condition(), compare(constant(0), constant(1)));
    EXPECT_TRUE(terms_of(rj).empty());
}

TEST_F(push_down_filters_test, join_relation_inner_keep_condition) {
    /*
     * scan:rl -\
     *           join_relation:rj - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(cl0), varref(cr0)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr0)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_over_left) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(l0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_over_left_at_most_one) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(l0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer_at_most_one,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_flush_right) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(r0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rj.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_flush_right_at_most_one) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(r0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer_at_most_one,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rj.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_condition_right) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r1 and r0=1):rj - ...
     * scan:rr -/
     * =>
     * scan:rl ---------------\
     *                         join_relation(left on:l0=r1):rj - ...
     * scan:rr - filter(r0=1) -/
     * for all left outer like joins (left outer, left outer at most one, and anti)
     */
    for (auto kind : {
            relation::join_kind::left_outer,
            relation::join_kind::left_outer_at_most_one,
            relation::join_kind::anti,
    }) {
        relation::graph_type r;
        auto cl0 = bindings.stream_variable("cl0");
        auto cl1 = bindings.stream_variable("cl1");
        auto cr0 = bindings.stream_variable("cr0");
        auto cr1 = bindings.stream_variable("cr1");
        auto&& rl = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, cl0 },
                        { t0c1, cl1 },
                },
        });
        auto&& rr = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t1c0, cr0 },
                        { t1c1, cr1 },
                },
        });
        auto&& rj = r.insert(relation::intermediate::join {
                kind,
                land(
                        compare(varref(cl0), varref(cr1)),
                        compare(varref(cr0), constant(1))),
        });
        rl.output() >> rj.left();
        rr.output() >> rj.right();

        connect(rj.output());
        apply(r);

        ASSERT_EQ(r.size(), 5) << kind;
        EXPECT_GT(rl.output(), rj.left()) << kind;
        auto&& f0 = next<relation::filter>(rr.output());
        EXPECT_GT(f0.output(), rj.right()) << kind;
        EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1))) << kind;
        auto terms = terms_of(rj);
        ASSERT_EQ(terms.size(), 1) << kind;
        EXPECT_EQ(*terms[0], compare(varref(cl0), varref(cr1))) << kind;
    }
}

TEST_F(push_down_filters_test, join_relation_left_outer_condition_left) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r1 and l1=1):rj - ...
     * scan:rr -/
     * => (not changed, because the left-only terms decide which left rows are padded with nulls)
     */
    for (auto kind : {
            relation::join_kind::left_outer,
            relation::join_kind::left_outer_at_most_one,
            relation::join_kind::anti,
    }) {
        relation::graph_type r;
        auto cl0 = bindings.stream_variable("cl0");
        auto cl1 = bindings.stream_variable("cl1");
        auto cr0 = bindings.stream_variable("cr0");
        auto cr1 = bindings.stream_variable("cr1");
        auto&& rl = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, cl0 },
                        { t0c1, cl1 },
                },
        });
        auto&& rr = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t1c0, cr0 },
                        { t1c1, cr1 },
                },
        });
        auto&& rj = r.insert(relation::intermediate::join {
                kind,
                land(
                        compare(varref(cl0), varref(cr1)),
                        compare(varref(cl1), constant(1))),
        });
        rl.output() >> rj.left();
        rr.output() >> rj.right();

        connect(rj.output());
        apply(r);

        ASSERT_EQ(r.size(), 4) << kind;
        EXPECT_GT(rl.output(), rj.left()) << kind;
        EXPECT_GT(rr.output(), rj.right()) << kind;
        EXPECT_EQ(rj.condition(), land(
                compare(varref(cl0), varref(cr1)),
                compare(varref(cl1), constant(1)))) << kind;
    }
}

TEST_F(push_down_filters_test, join_relation_left_outer_condition_constant) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r1 and 1=2):rj - ...
     * scan:rr -/
     * =>
     * scan:rl ---------------\
     *                         join_relation(left on:l0=r1):rj - ...
     * scan:rr - filter(1=2) --/
     * (the constant terms only restrict the right rows, and never remove the left rows)
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            land(
                    compare(varref(cl0), varref(cr1)),
                    compare(constant(1), constant(2))),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(rl.output(), rj.left());
    auto&& f0 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.right());
    EXPECT_EQ(f0.condition(), compare(constant(1), constant(2)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_condition_right_with_incoming) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r1 and r0=1):rj - filter(r1=2):rf - ...
     * scan:rr -/
     * =>
     * scan:rl ---------------\
     *                         join_relation(left on:l0=r1):rj - filter(r1=2) - ...
     * scan:rr - filter(r0=1) -/
     * (the incoming right-term conditions are still kept in the downstream of the join)
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            land(
                    compare(varref(cl0), varref(cr1)),
                    compare(varref(cr0), constant(1))),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr1), constant(2))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto&& f0 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj.right());
    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));

    auto&& f1 = next<relation::filter>(rj.output());
    EXPECT_EQ(f1.condition(), compare(varref(cr1), constant(2)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_semi_condition) {
    /*
     * scan:rl -\
     *           join_relation(semi on:l0=r0 and l1=1 and r1=2):rj - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(l1=1) -\
     *                          join_relation(semi on:l0=r0):rj - ...
     * scan:rr - filter(r1=2) -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::semi,
            land(
                    land(
                            compare(varref(cl0), varref(cr0)),
                            compare(varref(cl1), constant(1))),
                    compare(varref(cr1), constant(2))),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(1)));

    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_GT(f1.output(), rj.right());
    EXPECT_EQ(f1.condition(), compare(varref(cr1), constant(2)));

    auto terms = terms_of(rj);
    ASSERT_EQ(terms.size(), 1);
    EXPECT_EQ(*terms[0], compare(varref(cl0), varref(cr0)));
}

TEST_F(push_down_filters_test, join_relation_inner_condition_and_incoming) {
    /*
     * scan:rl -\
     *           join_relation(on:l0=r0 and l1=1):rj - filter(r1=2 and l0=r1):rf - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(l1=1) -\
     *                          join_relation(on:l0=r0 and l0=r1):rj - filter(TRUE):rf - ...
     * scan:rr - filter(r1=2) -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            land(
                    compare(varref(cl0), varref(cr0)),
                    compare(varref(cl1), constant(1))),
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    compare(varref(cr1), constant(2)),
                    compare(varref(cl0), varref(cr1))),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(1)));

    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_GT(f1.output(), rj.right());
    EXPECT_EQ(f1.condition(), compare(varref(cr1), constant(2)));

    auto terms = terms_of(rj);
    ASSERT_EQ(terms.size(), 2);
    EXPECT_TRUE(contains(terms, compare(varref(cl0), varref(cr0))));
    EXPECT_TRUE(contains(terms, compare(varref(cl0), varref(cr1))));
    EXPECT_TRUE(terms_of(rf).empty());
}

TEST_F(push_down_filters_test, join_relation_anti_over_left) {
    /*
     * scan:rl -\
     *           join_relation(anti on:l0=r0):rj - filter(l1=1):rf - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(l1=1) -\
     *                          join_relation(anti on:l0=r0):rj - filter(TRUE):rf - ...
     * scan:rr ----------------/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::anti,
            compare(varref(cl0), varref(cr0)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl1), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(1)));
    EXPECT_GT(rr.output(), rj.right());
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_left_outer_incoming_constant) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(1=2):rf - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(1=2) -\
     *                         join_relation(left on:l0=r0):rj - filter(TRUE):rf - ...
     * scan:rr ---------------/
     * (the incoming terms are never passed to the right input)
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(varref(cl0), varref(cr0)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(constant(1), constant(2)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_EQ(f0.condition(), compare(constant(1), constant(2)));
    EXPECT_GT(rr.output(), rj.right());
}

TEST_F(push_down_filters_test, join_relation_left_outer_without_condition) {
    /*
     * scan:rl -\
     *           join_relation(left):rj - filter(l1=1):rf - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(l1=1) -\
     *                          join_relation(left):rj - filter(TRUE):rf - ...
     * scan:rr ----------------/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl1), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(1)));
    EXPECT_TRUE(terms_of(rj).empty());
}

TEST_F(push_down_filters_test, join_find_flush) {
    /*
     * scan:r0 - join_find:r1 - filter(c0=1):rf - ...
     * =>
     * scan:r0 - join_find:r1 - filter(c0=1) - filter(TRUE):rf - ...
     * (the filters never go over index joins, because they are generated in the later phase)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto d0 = bindings.stream_variable("d0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& r1 = r.insert(relation::join_find {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t0c0, d0 },
            },
            {},
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(1)),
    });
    r0.output() >> r1.left();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.left());
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());
    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(1)));
}

TEST_F(push_down_filters_test, join_scan_flush) {
    /*
     * scan:r0 - join_scan:r1 - filter(c0=1):rf - ...
     * =>
     * scan:r0 - join_scan:r1 - filter(c0=1) - filter(TRUE):rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto d0 = bindings.stream_variable("d0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& r1 = r.insert(relation::join_scan {
            relation::join_kind::inner,
            bindings(*i0),
            {
                    { t0c0, d0 },
            },
            {},
            {},
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(1)),
    });
    r0.output() >> r1.left();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.left());
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());
    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(1)));
}

TEST_F(push_down_filters_test, filter_conjunction_partial) {
    /*
     * scan:r0 - project:r1 - filter(c0=1 and x0=2):rf - ...
     * =>
     * scan:r0 - filter(c0=1) - project:r1 - filter(x0=2) - filter(TRUE):rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto x0 = bindings.stream_variable("x0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    constant(1),
                    x0,
            },
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    compare(varref(c0), constant(1)),
                    compare(varref(x0), constant(2))),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());
    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(1)));

    auto&& f1 = next<relation::filter>(r1.output());
    EXPECT_GT(f1.output(), rf.input());
    EXPECT_EQ(f1.condition(), compare(varref(x0), constant(2)));
    EXPECT_TRUE(terms_of(rf).empty());
}

TEST_F(push_down_filters_test, filter_chain) {
    /*
     * scan:r0 - filter(c0=1):rf0 - filter(c1=2):rf1 - ...
     * =>
     * scan:r0 - filter - filter - filter(TRUE):rf0 - filter(TRUE):rf1 - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& rf0 = r.insert(relation::filter {
            compare(varref(c0), constant(1)),
    });
    auto&& rf1 = r.insert(relation::filter {
            compare(varref(c1), constant(2)),
    });
    r0.output() >> rf0.input();
    rf0.output() >> rf1.input();

    connect(rf1.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    auto&& f1 = next<relation::filter>(f0.output());
    EXPECT_GT(f1.output(), rf0.input());

    expression_list terms { std::addressof(f0.condition()), std::addressof(f1.condition()) };
    EXPECT_TRUE(contains(terms, compare(varref(c0), constant(1))));
    EXPECT_TRUE(contains(terms, compare(varref(c1), constant(2))));
    EXPECT_TRUE(terms_of(rf0).empty());
    EXPECT_TRUE(terms_of(rf1).empty());
}

TEST_F(push_down_filters_test, write) {
    /*
     * scan:r0 - filter(c0=1):rf - write:r1
     * =>
     * scan:r0 - filter(c0=1) - filter(TRUE):rf - write:r1
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(1)),
    });
    auto&& r1 = r.insert(relation::write {
            relation::write_kind::update,
            bindings(*i0),
            {
                    { c0, t0c0, },
            },
            {
                    { c1, t0c1, },
            },
    });
    r0.output() >> rf.input();
    rf.output() >> r1.input();

    apply(r);

    ASSERT_EQ(r.size(), 4);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());
    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_GT(rf.output(), r1.input());
}

TEST_F(push_down_filters_test, join_relation_semi_over_left) {
    /*
     * scan:rl -\
     *           join_relation(semi on:l0=r0):rj - filter(l1=1):rf - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(l1=1) -\
     *                          join_relation(semi on:l0=r0):rj - filter(TRUE):rf - ...
     * scan:rr ----------------/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::semi,
            compare(varref(cl0), varref(cr0)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl1), constant(1)),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(1)));
    EXPECT_GT(rr.output(), rj.right());
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, join_relation_inner_nested) {
    /*
     * scan:ra -\
     *           join_relation:rj0 -\
     * scan:rb -/                    join_relation:rj1 - filter(a1=1 and b1=2 and c1=3):rf - ...
     * scan:rc ---------------------/
     * =>
     * each term goes to just after its scan
     */
    relation::graph_type r;
    auto a0 = bindings.stream_variable("a0");
    auto a1 = bindings.stream_variable("a1");
    auto b0 = bindings.stream_variable("b0");
    auto b1 = bindings.stream_variable("b1");
    auto d0 = bindings.stream_variable("d0");
    auto d1 = bindings.stream_variable("d1");
    auto&& ra = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, a0 },
                    { t0c1, a1 },
            },
    });
    auto&& rb = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, b0 },
                    { t0c1, b1 },
            },
    });
    auto&& rc = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, d0 },
                    { t0c1, d1 },
            },
    });
    auto&& rj0 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(a0), varref(b0)),
    });
    auto&& rj1 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(a0), varref(d0)),
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    land(
                            compare(varref(a1), constant(1)),
                            compare(varref(b1), constant(2))),
                    compare(varref(d1), constant(3))),
    });
    ra.output() >> rj0.left();
    rb.output() >> rj0.right();
    rj0.output() >> rj1.left();
    rc.output() >> rj1.right();
    rj1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 10);
    auto&& f0 = next<relation::filter>(ra.output());
    EXPECT_GT(f0.output(), rj0.left());
    EXPECT_EQ(f0.condition(), compare(varref(a1), constant(1)));

    auto&& f1 = next<relation::filter>(rb.output());
    EXPECT_GT(f1.output(), rj0.right());
    EXPECT_EQ(f1.condition(), compare(varref(b1), constant(2)));

    auto&& f2 = next<relation::filter>(rc.output());
    EXPECT_GT(f2.output(), rj1.right());
    EXPECT_EQ(f2.condition(), compare(varref(d1), constant(3)));

    EXPECT_TRUE(terms_of(rf).empty());
}

TEST_F(push_down_filters_test, join_relation_left_outer_condition_right_over_join) {
    /*
     * scan:rl ---------------------\
     *                               join_relation(left on:l0=r0 and r1=1):rj1 - ...
     * scan:rr -\                   /
     *           join_relation:rj0 -
     * scan:rs -/
     * =>
     * scan:rr - filter(r1=1) - join_relation:rj0
     * (the right-only terms go further upstream of the right input)
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cs0 = bindings.stream_variable("cs0");
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
                    { t1c1, cr1 },
            },
    });
    auto&& rs = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c1, cs0 },
            },
    });
    auto&& rj0 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(cr0), varref(cs0)),
    });
    auto&& rj1 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            land(
                    compare(varref(cl0), varref(cr0)),
                    compare(varref(cr1), constant(1))),
    });
    rr.output() >> rj0.left();
    rs.output() >> rj0.right();
    rl.output() >> rj1.left();
    rj0.output() >> rj1.right();

    connect(rj1.output());
    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto&& f0 = next<relation::filter>(rr.output());
    EXPECT_GT(f0.output(), rj0.left());
    EXPECT_EQ(f0.condition(), compare(varref(cr1), constant(1)));

    auto terms = terms_of(rj1);
    ASSERT_EQ(terms.size(), 1);
    EXPECT_EQ(*terms[0], compare(varref(cl0), varref(cr0)));
}

TEST_F(push_down_filters_test, join_relation_left_outer_over_left_project) {
    /*
     * scan:rl - project:rp -\
     *                        join_relation(left on:l0=r0):rj - filter(l1=1 and x0=2):rf - ...
     * scan:rr --------------/
     * =>
     * scan:rl - filter(l1=1) - project:rp - filter(x0=2) -\
     *                                                      join_relation(left on:l0=r0):rj - filter(TRUE):rf - ...
     * scan:rr --------------------------------------------/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto x0 = bindings.stream_variable("x0");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rp = r.insert(relation::project {
            relation::project::column {
                    constant(1),
                    x0,
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(varref(cl0), varref(cr0)),
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    compare(varref(cl1), constant(1)),
                    compare(varref(x0), constant(2))),
    });
    rl.output() >> rp.input();
    rp.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 8);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), rp.input());
    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(1)));

    auto&& f1 = next<relation::filter>(rp.output());
    EXPECT_GT(f1.output(), rj.left());
    EXPECT_EQ(f1.condition(), compare(varref(x0), constant(2)));
    EXPECT_TRUE(terms_of(rf).empty());
}

TEST_F(push_down_filters_test, join_relation_inner_over_buffer) {
    /*
     * scan:r0 - buffer:r1 -     *                       join_relation(on:c0=d0):rj - filter(c1=1):rf - ...
     * scan:rs --------------/
     * =>
     * the term is placed just after the buffer, and never goes over it
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto d0 = bindings.stream_variable("d0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto&& r1 = r.insert(relation::buffer { 2 });
    auto&& rs = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, d0 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(c0), varref(d0)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c1), constant(1)),
    });
    r0.output() >> r1.input();
    r1.output_ports()[0] >> rj.left();
    rs.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    connect(r1.output_ports()[1]);
    apply(r);

    auto&& f0 = next<relation::filter>(r1.output_ports()[0]);
    EXPECT_GT(f0.output(), rj.left());
    EXPECT_EQ(f0.condition(), compare(varref(c1), constant(1)));
    EXPECT_GT(r0.output(), r1.input());
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, apply_mixed) {
    /*
     * scan:r0 - apply:r1 - filter(c0=0 and x0=1):rf - ...
     * =>
     * scan:r0 - filter(c0=0) - apply:r1 - filter(x0=1) - filter(TRUE):rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto x0 = bindings.stream_variable("x0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
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
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { c1 }
            },
            {
                    { 0, x0 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    compare(varref(c0), constant(0)),
                    compare(varref(x0), constant(1))),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());
    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));

    auto&& f1 = next<relation::filter>(r1.output());
    EXPECT_GT(f1.output(), rf.input());
    EXPECT_EQ(f1.condition(), compare(varref(x0), constant(1)));
    EXPECT_TRUE(terms_of(rf).empty());
}

TEST_F(push_down_filters_test, aggregate_relation_mixed) {
    /*
     * scan:r0 - aggregate_relation[c0]:r1 - filter(c0=0 and x0=1):rf - ...
     * =>
     * scan:r0 - filter(c0=0) - aggregate_relation[c0]:r1 - filter(x0=1) - filter(TRUE):rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    {
                            bindings(agg0),
                            { c1 },
                            x0,
                    },
            },
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    compare(varref(c0), constant(0)),
                    compare(varref(x0), constant(1))),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());
    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));

    auto&& f1 = next<relation::filter>(r1.output());
    EXPECT_GT(f1.output(), rf.input());
    EXPECT_EQ(f1.condition(), compare(varref(x0), constant(1)));
    EXPECT_TRUE(terms_of(rf).empty());
}

TEST_F(push_down_filters_test, join_relation_full_outer_condition_right) {
    /*
     * scan:rl -\
     *           join_relation(full on:l0=r1 and r0=1):rj - ...
     * scan:rr -/
     * => (not changed, because full outer joins also emit the right rows which are not joined)
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            land(
                    compare(varref(cl0), varref(cr1)),
                    compare(varref(cr0), constant(1))),
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 4);
    EXPECT_GT(rr.output(), rj.right());
    EXPECT_EQ(rj.condition(), land(
            compare(varref(cl0), varref(cr1)),
            compare(varref(cr0), constant(1))));
}

TEST_F(push_down_filters_test, join_relation_full_outer_flush_left) {
    /*
     * scan:rl -\
     *           join_relation(full on:l0=r0):rj - filter(l0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rj.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, join_relation_full_outer_flush_right) {
    /*
     * scan:rl -\
     *           join_relation(left on:l0=r0):rj - filter(r0=1):rf...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::full_outer,
            compare(varref(cl0), varref(cr1)),
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(1))
    });
    rl.output() >> rj.left();
    rr.output() >> rj.right();
    rj.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rj.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(1)));
    EXPECT_EQ(rf.condition(), boolean(true));
    EXPECT_EQ(rj.condition(), compare(varref(cl0), varref(cr1)));
}

TEST_F(push_down_filters_test, apply_over_left) {
    /*
     * scan:r0 - apply:r1 - filter:rf - ...
     * filter only refers left columns
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "o1", ::takatori::type::int8 {} },
                    { "o2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { cl1 }
            },
            {
                    { 0, cr0 },
                    { 1, cr1 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, apply_flush_use_right) {
    /*
     * scan:r0 - apply:r1 - filter:rf - ...
     * filter refers right columns
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "o1", ::takatori::type::int8 {} },
                    { "o2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { cl1 }
            },
            {
                    { 0, cr0 },
                    { 1, cr1 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cr0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cr0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, apply_flush_use_left_right) {
    /*
     * scan:r0 - apply:r1 - filter:rf - ...
     * filter refers left and right columns
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "o1", ::takatori::type::int8 {} },
                    { "o2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { cl1 }
            },
            {
                    { 0, cr0 },
                    { 1, cr1 },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl0), varref(cr0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(cl0), varref(cr0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, apply_join_relation_inner) {
    /*
     * scan:r0 - apply:r1 -\
     *                      join_relation:rj - ...
     * scan:rr ------------/
     * join condition refers the apply columns
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto ca0 = bindings.stream_variable("ca0");
    auto ca1 = bindings.stream_variable("ca1");
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "o1", ::takatori::type::int8 {} },
                    { "o2", ::takatori::type::int8 {} },
            },
            {
                    ::takatori::type::int8 {},
            },
            {
                    function::function_feature::table_valued_function,
            },
    });
    auto&& r1 = r.insert(relation::apply {
            tvf,
            {
                    scalar::variable_reference { cl1 }
            },
            {
                    { 0, ca0 },
                    { 1, ca1 },
            },
    });
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rr = r.insert(relation::scan {
            bindings(*i1),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto&& rj = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(varref(ca0), constant(1)),
    });
    r0.output() >> r1.input();
    r1.output() >> rj.left();
    rr.output() >> rj.right();

    connect(rj.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rj.left());

    EXPECT_EQ(f0.condition(), compare(varref(ca0), constant(1)));
    EXPECT_TRUE(terms_of(rj).empty());
}

TEST_F(push_down_filters_test, project_flush) {
    /*
     * scan:r0 - project:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c0 },
                            constant(1),
                    },
                    x0,
            },
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c1 },
                            constant(2),
                    },
                    x1,
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x1), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x1), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, project_pass) {
    /*
     * scan:r0 - project:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c0 },
                            constant(1),
                    },
                    x0,
            },
            relation::project::column {
                    scalar::binary {
                            scalar::binary_operator::add,
                            scalar::variable_reference { c1 },
                            constant(2),
                    },
                    x1,
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, buffer) {
    /*
     *                    /- filter:rf - ...
     * scan:r0 - buffer:r1
     *                    \- ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto&& r1 = r.insert(relation::buffer { 2 });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output_ports()[0] >> rf.input();

    connect(rf.output());
    connect(r1.output_ports()[1]);
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r1.output_ports()[0]);
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, identify_flush) {
    /*
     * scan:r0 - identify:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::identify {
            x0,
            t::row_id { 2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x0), varref(x0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x0), varref(x0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, identify_pass) {
    /*
     * scan:r0 - identify:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto&& r1 = r.insert(relation::identify {
            x0,
            t::row_id { 2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, aggregate_relation_flush) {
    /*
     * scan:r0 - aggregate_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x1 = bindings.stream_variable("c3");
    auto x2 = bindings.stream_variable("c3");
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    {
                            bindings(agg0),
                            { c1 },
                            x1,
                    },
                    {
                            bindings(agg1),
                            { c2 },
                            x2,
                    },

            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x1), varref(c0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x1), varref(c0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, aggregate_relation_through) {
    /*
     * scan:r0 - aggregate_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x1 = bindings.stream_variable("c3");
    auto x2 = bindings.stream_variable("c3");
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {
                    c0,
            },
            {
                    {
                            bindings(agg0),
                            { c1 },
                            x1,
                    },
                    {
                            bindings(agg1),
                            { c2 },
                            x2,
                    },

            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, aggregate_relation_without_group_keys_constant) {
    /*
     * scan:r0 - aggregate_relation[]:r1 - filter(1=2):rf - ...
     * =>
     * scan:r0 - aggregate_relation[]:r1 - filter(1=2) - filter(TRUE):rf - ...
     * (the aggregation without group keys emits a row even if its input is empty)
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto& r1 = r.insert(relation::intermediate::aggregate {
            {},
            {
                    {
                            bindings(agg0),
                            { c0 },
                            x0,
                    },
            },
    });
    auto&& rf = r.insert(relation::filter {
            compare(constant(1), constant(2)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());
    EXPECT_EQ(f0.condition(), compare(constant(1), constant(2)));
}

TEST_F(push_down_filters_test, distinct_relation_flush) {
    /*
     * scan:r0 - distinct_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::distinct {
            c0,
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), varref(c0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), varref(c0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, distinct_relation_pass) {
    /*
     * scan:r0 - distinct_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::distinct {
            c0,
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, limit_relation_flush) {
    /*
     * scan:r0 - limit_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::limit {
            { c0, },
            { c1, },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c2), varref(c0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    // NOTE: the filter must not go over the limit, because it changes which rows are in the row slice
    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(c2), varref(c0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, limit_relation_flush_without_group_keys) {
    /*
     * scan:r0 - limit_relation:r1[c1](3) - filter(c0=0):rf - ...
     * =>
     * scan:r0 - limit_relation:r1[c1](3) - filter(c0=0) - filter(TRUE):rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::limit {
            {},
            { c1, },
            relation::intermediate::limit::row_slice_type { {}, 3 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    EXPECT_GT(r0.output(), r1.input());
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, limit_relation_pass) {
    /*
     * scan:r0 - limit_relation:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto& r1 = r.insert(relation::intermediate::limit {
            { c0, },
            { c1, },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(c0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), r1.input());

    EXPECT_EQ(f0.condition(), compare(varref(c0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, union_relation_both) {
    /*
     * scan:rl -\
     *           union_relation:r0 - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto&& r0 = r.insert(relation::intermediate::union_ {
            { cl0, cl0, x0 },
            { cl1,  {}, x1 },
            {  {}, cl1, x2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x1), varref(x2)),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(r0.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x1), varref(x2)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, union_relation_pass) {
    /*
     * scan:rl -\
     *           union_relation[all]:r0 - filter(x0=1):rf - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(cl0=1) -\
     *                           union_relation[all]:r0 - filter(TRUE):rf - ...
     * scan:rr - filter(cr0=1) -/
     * for both set quantifiers
     */
    for (auto quantifier : { relation::set_quantifier::all, relation::set_quantifier::distinct }) {
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
        auto x0 = bindings.stream_variable("x0");
        auto&& r0 = r.insert(relation::intermediate::union_ {
                {
                        { cl0, cr0, x0 },
                },
                quantifier,
        });
        auto&& rf = r.insert(relation::filter {
                compare(varref(x0), constant(1)),
        });
        rl.output() >> r0.left();
        rr.output() >> r0.right();
        r0.output() >> rf.input();

        connect(rf.output());
        apply(r);

        ASSERT_EQ(r.size(), 7) << quantifier;
        auto&& f0 = next<relation::filter>(rl.output());
        EXPECT_GT(f0.output(), r0.left()) << quantifier;
        EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1))) << quantifier;

        auto&& f1 = next<relation::filter>(rr.output());
        EXPECT_GT(f1.output(), r0.right()) << quantifier;
        EXPECT_EQ(f1.condition(), compare(varref(cr0), constant(1))) << quantifier;

        EXPECT_EQ(rf.condition(), boolean(true)) << quantifier;
    }
}

TEST_F(push_down_filters_test, union_relation_absent_column) {
    /*
     * scan:rl -\
     *           union_relation[all]:r0 - filter(x0=1 and x1=2):rf - ...
     * scan:rr -/
     * (x1 is absent in the right input)
     * =>
     * scan:rl - filter(cl0=1) -\
     *                           union_relation[all]:r0 - filter(x1=2) - filter(TRUE):rf - ...
     * scan:rr - filter(cr0=1) -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r0 = r.insert(relation::intermediate::union_ {
            {
                    { cl0, cr0, x0 },
                    { cl1,  {}, x1 },
            },
            relation::set_quantifier::all,
    });
    auto&& rf = r.insert(relation::filter {
            land(
                    compare(varref(x0), constant(1)),
                    compare(varref(x1), constant(2))),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 8);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), r0.left());
    EXPECT_EQ(f0.condition(), compare(varref(cl0), constant(1)));

    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_GT(f1.output(), r0.right());
    EXPECT_EQ(f1.condition(), compare(varref(cr0), constant(1)));

    auto&& f2 = next<relation::filter>(r0.output());
    EXPECT_GT(f2.output(), rf.input());
    EXPECT_EQ(f2.condition(), compare(varref(x1), constant(2)));
    EXPECT_TRUE(terms_of(rf).empty());
}

TEST_F(push_down_filters_test, union_relation_multiple_columns) {
    /*
     * scan:rl -\
     *           union_relation[all]:r0 - filter(x0=x1):rf - ...
     * scan:rr -/
     * =>
     * scan:rl - filter(cl0=cl1) -\
     *                             union_relation[all]:r0 - filter(TRUE):rf - ...
     * scan:rr - filter(cr1=cr0) -/
     * (the right input has the different column order)
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto&& r0 = r.insert(relation::intermediate::union_ {
            {
                    { cl0, cr1, x0 },
                    { cl1, cr0, x1 },
            },
            relation::set_quantifier::all,
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x0), varref(x1)),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 7);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_EQ(f0.condition(), compare(varref(cl0), varref(cl1)));
    auto&& f1 = next<relation::filter>(rr.output());
    EXPECT_EQ(f1.condition(), compare(varref(cr1), varref(cr0)));
}

TEST_F(push_down_filters_test, union_relation_nested) {
    /*
     * scan:ra -\
     *           union_relation:r0 -\
     * scan:rb -/                    union_relation:r1 - filter(y0=1):rf - ...
     * scan:rc ---------------------/
     * =>
     * the term goes to each scan
     */
    relation::graph_type r;
    auto a0 = bindings.stream_variable("a0");
    auto b0 = bindings.stream_variable("b0");
    auto d0 = bindings.stream_variable("d0");
    auto&& ra = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, a0 },
            },
    });
    auto&& rb = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, b0 },
            },
    });
    auto&& rc = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, d0 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto y0 = bindings.stream_variable("y0");
    auto&& r0 = r.insert(relation::intermediate::union_ {
            {
                    { a0, b0, x0 },
            },
            relation::set_quantifier::all,
    });
    auto&& r1 = r.insert(relation::intermediate::union_ {
            {
                    { x0, d0, y0 },
            },
            relation::set_quantifier::all,
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(y0), constant(1)),
    });
    ra.output() >> r0.left();
    rb.output() >> r0.right();
    r0.output() >> r1.left();
    rc.output() >> r1.right();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 10);
    EXPECT_EQ(next<relation::filter>(ra.output()).condition(), compare(varref(a0), constant(1)));
    EXPECT_EQ(next<relation::filter>(rb.output()).condition(), compare(varref(b0), constant(1)));
    EXPECT_EQ(next<relation::filter>(rc.output()).condition(), compare(varref(d0), constant(1)));
}

TEST_F(push_down_filters_test, intersection_relation) {
    /*
     * scan:rl -\
     *           intersection_relation:r0 - filter:rf - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto&& r0 = r.insert(relation::intermediate::intersection {
            { cl0, cr0 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl1), constant(0)),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), r0.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, difference_relation) {
    /*
     * scan:rl -\
     *           difference_relation:r0 - filter:rd - ...
     * scan:rr -/
     */
    relation::graph_type r;
    auto cl0 = bindings.stream_variable("cl0");
    auto cl1 = bindings.stream_variable("cl1");
    auto cl2 = bindings.stream_variable("cl2");
    auto cr0 = bindings.stream_variable("cr0");
    auto cr1 = bindings.stream_variable("cr1");
    auto cr2 = bindings.stream_variable("cr2");
    auto&& rl = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, cl0 },
                    { t0c1, cl1 },
                    { t0c2, cl2 },
            },
    });
    auto&& rr = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t1c0, cr0 },
                    { t1c1, cr1 },
                    { t1c2, cr2 },
            },
    });
    auto&& r0 = r.insert(relation::intermediate::difference {
            { cl0, cr0 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(cl1), constant(0)),
    });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 6);
    auto&& f0 = next<relation::filter>(rl.output());
    EXPECT_GT(f0.output(), r0.left());

    EXPECT_EQ(f0.condition(), compare(varref(cl1), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

TEST_F(push_down_filters_test, escape) {
    /*
     * scan:r0 - escape:r1 - filter:rf - ...
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto c2 = bindings.stream_variable("c2");
    auto&& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
                    { t0c2, c2 },
            },
    });
    auto x0 = bindings.stream_variable("x0");
    auto x1 = bindings.stream_variable("x1");
    auto x2 = bindings.stream_variable("x2");
    auto&& r1 = r.insert(relation::intermediate::escape {
            { c0, x0 },
            { c1, x1 },
            { c2, x2 },
    });
    auto&& rf = r.insert(relation::filter {
            compare(varref(x0), constant(0)),
    });
    r0.output() >> r1.input();
    r1.output() >> rf.input();

    connect(rf.output());
    apply(r);

    ASSERT_EQ(r.size(), 5);
    auto&& f0 = next<relation::filter>(r1.output());
    EXPECT_GT(f0.output(), rf.input());

    EXPECT_EQ(f0.condition(), compare(varref(x0), constant(0)));
    EXPECT_EQ(rf.condition(), boolean(true));
}

} // namespace yugawara::analyzer::details
