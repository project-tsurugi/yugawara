#include <yugawara/analyzer/details/decompose_disjunction_range.h>

#include <gtest/gtest.h>

#include <takatori/relation/scan.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/emit.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/analyzer/details/decompose_predicate.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using cmp = scalar::comparison_operator;

class decompose_disjunction_range_test : public ::testing::Test {
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

    void apply(relation::graph_type& graph) {
        decompose_disjunction_range(graph);
    }

    std::vector<scalar::expression const*> decompose_conjunctions(relation::filter& op) {
        std::vector<scalar::expression const*> results {};
        decompose_predicate(op.condition(), [&](auto&& expr) -> void {
            results.push_back(std::addressof(expr.get()));
        });
        return results;
    }

    bool contains_expression(
            std::vector<scalar::expression const*> const& expressions,
            scalar::expression const& target) {
        return std::any_of(expressions.begin(), expressions.end(), [&](scalar::expression const* expr) -> bool {
            return *expr == target;
        });
    }

    std::string to_string(std::vector<scalar::expression const*> const& expressions) {
        string_builder builder;
        for (auto&& expr : expressions) {
            builder << *expr << "\n";
        }
        return builder.str();
    }
};

TEST_F(decompose_disjunction_range_test, simple) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = 0 }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
            compare(varref { c0 }, constant(0)),
    });
    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r0));

    auto&& f0 = next<relation::filter>(r0.output());
    ASSERT_TRUE(r.contains(f0));

    EXPECT_GT(f0.output(), ro.input());

    EXPECT_EQ(f0.condition(), compare(varref { c0 }, constant(0)));
}

TEST_F(decompose_disjunction_range_test, disjunction) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = 0 or c0 = 1 }
     * -> filter { (0 <= c0 and c0 <= 1) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, constant(0)),
            compare(varref { c0 }, constant(1)))
    });


    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r0));
    ASSERT_TRUE(r.contains(r1));
    ASSERT_TRUE(r.contains(ro));

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 3);
    // original condition
    EXPECT_TRUE(contains_expression(terms, lor(
                    compare(varref { c0 }, constant(0)),
                    compare(varref { c0 }, constant(1)))));
    // 0 <= c0
    EXPECT_TRUE(contains_expression(terms, compare(constant(0), varref { c0 }, cmp::less_equal)));
    // c0 <= 1
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, constant(1), cmp::less_equal)));
}

TEST_F(decompose_disjunction_range_test, disjunction_union_range) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { 0 < c0 and c0 < 1 or 2 < c0 and c0 < 3 }
     * -> filter { (0 < c0 and c0 < 3) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            land(
                compare(constant(0), varref { c0 }, cmp::less),
                compare(varref { c0 }, constant(1), cmp::less)),
            land(
                compare(constant(2), varref { c0 }, cmp::less),
                compare(varref { c0 }, constant(3), cmp::less)))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r0));
    ASSERT_TRUE(r.contains(r1));
    ASSERT_TRUE(r.contains(ro));

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 3);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // 0 < c0
    EXPECT_TRUE(contains_expression(terms, compare(constant(0), varref { c0 }, cmp::less))) << to_string(terms);
    // c0 < 3
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, constant(3), cmp::less)));
}

TEST_F(decompose_disjunction_range_test, disjunction_half_range) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 < 0 or c0 = 0 and c1 <= 0 }
     * -> filter { (c0 <= 0) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, constant(0), cmp::less),
            land(
                compare(varref { c0 }, constant(0), cmp::equal),
                compare(varref { c1 }, constant(0), cmp::less_equal)))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r0));
    ASSERT_TRUE(r.contains(r1));
    ASSERT_TRUE(r.contains(ro));

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // c0 <= 0
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, constant(0), cmp::less_equal)));
}

TEST_F(decompose_disjunction_range_test, disjunction_variable) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 < :v0 or c0 = :v0 and c1 <= :v1 }
     * -> filter { (c0 <= 0) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c1");
    auto v0 = bindings.external_variable({"v0", t::int4()});
    auto v1 = bindings.external_variable({"v1", t::int4()});

    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, varref { v0 }, cmp::less),
            land(
                compare(varref { c0 }, varref { v0 }, cmp::equal),
                compare(varref { c1 }, varref { v1 }, cmp::less_equal)))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    ASSERT_EQ(r.size(), 3);
    ASSERT_TRUE(r.contains(r0));
    ASSERT_TRUE(r.contains(r1));
    ASSERT_TRUE(r.contains(ro));

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // c0 <= :v0
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, varref { v0 }, cmp::less_equal)));
}

TEST_F(decompose_disjunction_range_test, immediate_equal) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = 0 or 1 = c0 }
     * -> filter { (0 <= c0 and c0 <= 1) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, constant(0)),
            compare(constant(1), varref { c0 }))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 3);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // 0 <= c0
    EXPECT_TRUE(contains_expression(terms, compare(constant(0), varref { c0 }, cmp::less_equal)));
    // c0 <= 1
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, constant(1), cmp::less_equal)));
}

TEST_F(decompose_disjunction_range_test, immediate_less) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 < 0 or 1 > c0 }
     * -> filter { (c0 < 1) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, constant(0), cmp::less),
            compare(constant(1), varref { c0 }, cmp::greater))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // c0 < 1
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, constant(1), cmp::less)));
}

TEST_F(decompose_disjunction_range_test, immediate_greater) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 > 0 or 1 < c0 }
     * -> filter { (c0 > 0) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, constant(0), cmp::greater),
            compare(constant(1), varref { c0 }, cmp::less))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // 0 < c0
    EXPECT_TRUE(contains_expression(terms, compare(constant(0), varref { c0 }, cmp::less)));
}

TEST_F(decompose_disjunction_range_test, immediate_less_equal) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 <= 0 or 1 >= c0 }
     * -> filter { (c0 <= 1) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, constant(0), cmp::less_equal),
            compare(constant(1), varref { c0 }, cmp::greater_equal))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // c0 <= 1
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, constant(1), cmp::less_equal)));
}

TEST_F(decompose_disjunction_range_test, immediate_greater_equal) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 >= 0 or 1 <= c0 }
     * -> filter { (c0 >= 0) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, constant(0), cmp::greater_equal),
            compare(constant(1), varref { c0 }, cmp::less_equal))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // 0 <= c0
    EXPECT_TRUE(contains_expression(terms, compare(constant(0), varref { c0 }, cmp::less_equal))) << to_string(terms);
}

TEST_F(decompose_disjunction_range_test, variable_equal) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 = v or v = c0 }
     * -> filter { (v < c0 and c0 <= v) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto v = bindings.external_variable({ "v", t::int4() });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, varref { v }),
            compare(varref { v }, varref { c0 }))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 3);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // v <= c0
    EXPECT_TRUE(contains_expression(terms, compare(varref { v }, varref { c0 }, cmp::less_equal)));
    // c0 <= v
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, varref { v }, cmp::less_equal)));
}

TEST_F(decompose_disjunction_range_test, variable_less) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 < v or v > c0 }
     * -> filter { (c0 < v) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto v = bindings.external_variable({ "v", t::int4() });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, varref { v }, cmp::less),
            compare(varref { v }, varref { c0 }, cmp::greater))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // c0 < v
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, varref { v }, cmp::less)));
}

TEST_F(decompose_disjunction_range_test, variable_greater) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 > v or v < c0 }
     * -> filter { (v > c0) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto v = bindings.external_variable({ "v", t::int4() });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, varref { v }, cmp::greater),
            compare(varref { v }, varref { c0 }, cmp::less))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // v > c0
    EXPECT_TRUE(contains_expression(terms, compare(varref { v }, varref { c0 }, cmp::less)));
}

TEST_F(decompose_disjunction_range_test, variable_less_equal) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 <= v or v > c0 }
     * -> filter { (c0 <= v) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto v = bindings.external_variable({ "v", t::int4() });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, varref { v }, cmp::less_equal),
            compare(varref { v }, varref { c0 }, cmp::greater))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // c0 <= v
    EXPECT_TRUE(contains_expression(terms, compare(varref { c0 }, varref { v }, cmp::less_equal)));
}

TEST_F(decompose_disjunction_range_test, variable_greater_equal) {
    /*
     * scan:r0 - filter:r1 - emit:ro
     * filter { c0 >= v or v < c0 }
     * -> filter { (v >= c0) and <original> }
     */
    relation::graph_type r;
    auto c0 = bindings.stream_variable("c0");
    auto& r0 = r.insert(relation::scan {
            bindings(*i0),
            {
                    { t0c0, c0 },
            },
    });
    auto v = bindings.external_variable({ "v", t::int4() });
    auto& r1 = r.insert(relation::filter {
        lor(
            compare(varref { c0 }, varref { v }, cmp::greater_equal),
            compare(varref { v }, varref { c0 }, cmp::less))
    });
    auto origin = ::takatori::util::clone_unique(r1.condition());

    auto& ro = r.insert(relation::emit {
            c0,
    });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply(r);

    auto terms = decompose_conjunctions(r1);
    EXPECT_EQ(terms.size(), 2);
    // original condition
    EXPECT_TRUE(contains_expression(terms, *origin));
    // v >= c0
    EXPECT_TRUE(contains_expression(terms, compare(varref { v }, varref { c0 }, cmp::less_equal)));
}

} // namespace yugawara::analyzer::details
