#include <yugawara/analyzer/details/remove_unused_stream_variables.h>

#include <algorithm>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/table.h>

#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>
#include <takatori/scalar/let.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/join_scan.h>
#include <takatori/relation/join_find.h>
#include <takatori/relation/apply.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>
#include <takatori/relation/buffer.h>
#include <takatori/relation/identify.h>
#include <takatori/relation/emit.h>
#include <takatori/relation/write.h>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>
#include <takatori/relation/intermediate/distinct.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/intersection.h>
#include <takatori/relation/intermediate/difference.h>
#include <takatori/relation/intermediate/union.h>
#include <takatori/relation/intermediate/escape.h>

#include <takatori/util/downcast.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>
#include <yugawara/storage/configurable_provider.h>
#include <yugawara/aggregate/configurable_provider.h>

#include <yugawara/testing/utils.h>

/*
 * Contracts of remove_unused_stream_variables.
 *
 * The pass removes columns and operators that nothing refers to. The removal must not change the query result.
 * The comment before each test, or each group of tests, states the contract.
 *
 * A column that "counts as referred" stays in the operators upstream.
 * Whether a column is referred is decided once for the whole graph, not for each path.
 * The order of the remaining columns is not a contract, so columns are compared as sets.
 */
namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::util::unsafe_downcast;

namespace {

using vars = std::vector<descriptor::variable>;

bool same(vars a, vars const& b) {
    return a.size() == b.size() && std::is_permutation(a.begin(), a.end(), b.begin());
}

template<class Columns>
vars destinations(Columns const& columns) {
    vars result {};
    for (auto&& c : columns) {
        result.emplace_back(c.destination());
    }
    return result;
}

template<class Columns>
vars variables(Columns const& columns) {
    vars result {};
    for (auto&& c : columns) {
        result.emplace_back(c.variable());
    }
    return result;
}

template<class Output, class Input>
bool connected(Output const& output, Input const& input) {
    auto opposite = output.opposite();
    return opposite && std::addressof(*opposite) == std::addressof(input);
}

relation::emit no_columns() {
    return relation::emit { std::vector<relation::emit::column> {} };
}

} // namespace

class remove_unused_stream_variables_contract_test : public ::testing::Test {
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

    descriptor::variable c0 = bindings.stream_variable("c0");
    descriptor::variable c1 = bindings.stream_variable("c1");
    descriptor::variable c2 = bindings.stream_variable("c2");
    descriptor::variable d0 = bindings.stream_variable("d0");
    descriptor::variable d1 = bindings.stream_variable("d1");
    descriptor::variable d2 = bindings.stream_variable("d2");
    descriptor::variable x0 = bindings.stream_variable("x0");
    descriptor::variable x1 = bindings.stream_variable("x1");
    descriptor::variable x2 = bindings.stream_variable("x2");

    relation::graph_type r;

    relation::scan& left_scan() {
        return r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                        { t0c2, c2 },
                },
        });
    }

    relation::scan& right_scan() {
        return r.insert(relation::scan {
                bindings(*i1),
                {
                        { t1c0, d0 },
                        { t1c1, d1 },
                        { t1c2, d2 },
                },
        });
    }

    relation::intermediate::union_& union_of(relation::set_quantifier quantifier) {
        return r.insert(relation::intermediate::union_ {
                {
                        { c0, d0, x0 },
                        { c1, d1, x1 },
                        { c2, d2, x2 },
                },
                quantifier,
        });
    }

    template<class Op>
    void check_set_operation(relation::set_quantifier quantifier) {
        // scans - set operation - emit
        auto&& rl = left_scan();
        auto&& rr = right_scan();
        auto&& r0 = r.insert(Op {
                {
                        { c0, d0 },
                        { c1, d1 },
                },
                quantifier,
        });
        auto&& ro = r.insert(relation::emit { c0 });
        rl.output() >> r0.left();
        rr.output() >> r0.right();
        r0.output() >> ro.input();

        apply();

        EXPECT_EQ(r0.group_key_pairs().size(), 2);
        EXPECT_TRUE(same(destinations(rl.columns()), { c0, c1 }));
        EXPECT_TRUE(same(destinations(rr.columns()), { d0, d1 }));
    }

    void check_join_keeps_right(relation::join_kind kind) {
        auto&& rl = left_scan();
        auto&& rr = right_scan();
        auto&& r0 = r.insert(relation::intermediate::join { kind });
        auto&& ro = r.insert(relation::emit { c0 });
        rl.output() >> r0.left();
        rr.output() >> r0.right();
        r0.output() >> ro.input();

        apply();

        ASSERT_EQ(r.size(), 4);
        EXPECT_TRUE(connected(rl.output(), r0.left()));
        EXPECT_TRUE(connected(rr.output(), r0.right()));
        EXPECT_TRUE(rr.columns().empty());
    }

    void apply() {
        details::remove_unused_stream_variables(r);
    }
};

// UNION DISTINCT finds duplicates using all columns.
// All input columns count as referred, and no mapping is removed, even if nothing downstream uses it.
TEST_F(remove_unused_stream_variables_contract_test, union_distinct_keeps_unused_columns) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& r0 = union_of(relation::set_quantifier::distinct);
    auto&& ro = r.insert(relation::emit { x0 });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    // every column decides duplicates
    EXPECT_EQ(r0.mappings().size(), 3);
    EXPECT_EQ(rl.columns().size(), 3);
    EXPECT_EQ(rr.columns().size(), 3);
}

// UNION ALL does not look for duplicates. Only the columns referred downstream count as referred.
TEST_F(remove_unused_stream_variables_contract_test, union_all_uses_only_referred_columns) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& r0 = union_of(relation::set_quantifier::all);
    auto&& ro = r.insert(relation::emit { x1 });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r0.mappings().size(), 1);
    EXPECT_EQ(r0.mappings()[0].destination(), x1);
    EXPECT_TRUE(same(destinations(rl.columns()), { c1 }));
    EXPECT_TRUE(same(destinations(rr.columns()), { d1 }));
}

// UNION ALL yields the rows of both inputs.
// If nothing downstream refers to its columns, the mappings are removed, but the operator and both inputs stay.
TEST_F(remove_unused_stream_variables_contract_test, union_all_without_referred_columns_is_kept) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& r0 = union_of(relation::set_quantifier::all);
    auto&& ro = r.insert(no_columns());
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    // the number of rows is the sum of both inputs
    ASSERT_EQ(r.size(), 4);
    EXPECT_TRUE(connected(rl.output(), r0.left()));
    EXPECT_TRUE(connected(rr.output(), r0.right()));
    EXPECT_TRUE(connected(r0.output(), ro.input()));
    EXPECT_TRUE(r0.mappings().empty());
}

// DISTINCT finds duplicates using all group keys.
// The group keys count as referred and stay, even if nothing downstream uses them.
TEST_F(remove_unused_stream_variables_contract_test, distinct_keeps_group_keys) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::distinct { c0, c1 });
    auto&& ro = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    EXPECT_EQ(r1.group_keys().size(), 2);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1 }));
}

// INTERSECT and EXCEPT match rows using all columns, both with DISTINCT and with ALL.
// Both sides of each group key pair count as referred.
TEST_F(remove_unused_stream_variables_contract_test, intersection_distinct_keeps_both_sides_of_keys) {
    check_set_operation<relation::intermediate::intersection>(relation::set_quantifier::distinct);
}

TEST_F(remove_unused_stream_variables_contract_test, difference_distinct_keeps_both_sides_of_keys) {
    check_set_operation<relation::intermediate::difference>(relation::set_quantifier::distinct);
}

TEST_F(remove_unused_stream_variables_contract_test, intersection_all_keeps_both_sides_of_keys) {
    check_set_operation<relation::intermediate::intersection>(relation::set_quantifier::all);
}

TEST_F(remove_unused_stream_variables_contract_test, difference_all_keeps_both_sides_of_keys) {
    check_set_operation<relation::intermediate::difference>(relation::set_quantifier::all);
}

// Aggregate columns that nothing downstream refers to are removed, but the aggregate stays.
// Without group keys it yields one row even for an empty input. With group keys it yields one row for each group.
TEST_F(remove_unused_stream_variables_contract_test, aggregate_without_keys_is_kept) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::aggregate {
            {},
            {
                    { bindings(agg0), { c1 }, x0 },
            },
    });
    auto&& ro = r.insert(no_columns());
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    // yields one row even if the input is empty
    ASSERT_EQ(r.size(), 3);
    EXPECT_TRUE(connected(r0.output(), r1.input()));
    EXPECT_TRUE(connected(r1.output(), ro.input()));
    EXPECT_TRUE(r1.columns().empty());
}

TEST_F(remove_unused_stream_variables_contract_test, aggregate_with_keys_is_kept) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::aggregate {
            { c0 },
            {
                    { bindings(agg0), { c1 }, x0 },
            },
    });
    auto&& ro = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    // yields one row for each group
    ASSERT_EQ(r.size(), 3);
    EXPECT_TRUE(r1.columns().empty());
    EXPECT_TRUE(same(destinations(r0.columns()), { c0 }));
}

// The group keys of an aggregate decide the groups, so they count as referred.
TEST_F(remove_unused_stream_variables_contract_test, aggregate_keeps_group_keys) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::aggregate {
            { c0 },
            {
                    { bindings(agg0), { c1 }, x0 },
            },
    });
    auto&& ro = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    EXPECT_EQ(r1.group_keys().size(), 1);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1 }));
}

// The arguments of a referred aggregate column count as referred.
// The arguments of a removed aggregate column do not.
TEST_F(remove_unused_stream_variables_contract_test, aggregate_arguments_follow_remaining_columns) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::aggregate {
            {},
            {
                    { bindings(agg0), { c1 }, x0 },
                    { bindings(agg1), { c2 }, x1 },
            },
    });
    auto&& ro = r.insert(relation::emit { x1 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(destinations(r1.columns()), { x1 }));
    EXPECT_TRUE(same(destinations(r0.columns()), { c2 }));
}

// The sort keys of a limit decide which rows remain, so they count as referred.
TEST_F(remove_unused_stream_variables_contract_test, limit_keeps_sort_keys) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::limit {
            {},
            { c1 },
    });
    auto&& ro = r.insert(relation::emit { c2 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    EXPECT_EQ(r1.sort_keys().size(), 1);
    EXPECT_TRUE(same(destinations(r0.columns()), { c1, c2 }));
}

// The group keys of a limit decide the unit of the limit, so they count as referred.
TEST_F(remove_unused_stream_variables_contract_test, limit_keeps_group_keys) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::limit {
            { c0 },
            {},
    });
    auto&& ro = r.insert(relation::emit { c2 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    EXPECT_EQ(r1.group_keys().size(), 1);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c2 }));
}

// A limit changes the number of rows.
// The limit and its keys stay, even if nothing downstream refers to any column.
TEST_F(remove_unused_stream_variables_contract_test, limit_without_referred_columns_is_kept) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::limit {
            { c0 },
            { c1 },
    });
    auto&& ro = r.insert(no_columns());
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 3);
    EXPECT_TRUE(connected(r0.output(), r1.input()));
    EXPECT_TRUE(connected(r1.output(), ro.input()));
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1 }));
}

// The condition and the lower and upper keys of a join decide which rows are combined.
// Columns in those expressions count as referred.
TEST_F(remove_unused_stream_variables_contract_test, join_condition_uses_columns) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::inner,
            compare(c1, d1),
    });
    auto&& ro = r.insert(no_columns());
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(destinations(rl.columns()), { c1 }));
    EXPECT_TRUE(same(destinations(rr.columns()), { d1 }));
}

TEST_F(remove_unused_stream_variables_contract_test, join_lower_key_uses_columns) {
    using join = relation::intermediate::join;
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& r0 = r.insert(join {
            relation::join_kind::inner,
            join::endpoint {
                    join::key { d0, scalar::variable_reference { c0 } },
                    relation::endpoint_kind::inclusive,
            },
            join::endpoint {},
            {},
    });
    auto&& ro = r.insert(no_columns());
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(destinations(rl.columns()), { c0 }));
}

TEST_F(remove_unused_stream_variables_contract_test, join_upper_key_uses_columns) {
    using join = relation::intermediate::join;
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& r0 = r.insert(join {
            relation::join_kind::inner,
            join::endpoint {},
            join::endpoint {
                    join::key { d0, scalar::variable_reference { c1 } },
                    relation::endpoint_kind::inclusive,
            },
            {},
    });
    auto&& ro = r.insert(no_columns());
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(destinations(rl.columns()), { c1 }));
}

// Whether right rows match, and how many, changes the output rows.
// For every join kind, the join and its right input stay, even if nothing downstream refers to the right columns.
TEST_F(remove_unused_stream_variables_contract_test, join_keeps_right_inner) {
    check_join_keeps_right(relation::join_kind::inner);
}

TEST_F(remove_unused_stream_variables_contract_test, join_keeps_right_left_outer) {
    check_join_keeps_right(relation::join_kind::left_outer);
}

TEST_F(remove_unused_stream_variables_contract_test, join_keeps_right_full_outer) {
    check_join_keeps_right(relation::join_kind::full_outer);
}

TEST_F(remove_unused_stream_variables_contract_test, join_keeps_right_semi) {
    check_join_keeps_right(relation::join_kind::semi);
}

TEST_F(remove_unused_stream_variables_contract_test, join_keeps_right_anti) {
    check_join_keeps_right(relation::join_kind::anti);
}

// A project on the null-supplying side of an outer join stays on that side if its column is referred downstream.
// This holds even for a constant column: the pass does not propagate constants, and unmatched rows must get NULL.
TEST_F(remove_unused_stream_variables_contract_test, project_on_the_null_supplying_side_stays_there) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& rp = r.insert(relation::project {
            relation::project::column { constant(1), x0 },
    });
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(c0, d0),
    });
    auto&& ro = r.insert(relation::emit { c1, x0 });
    rl.output() >> r0.left();
    rr.output() >> rp.input();
    rp.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    // the column must be null for the rows without matches
    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(connected(rr.output(), rp.input()));
    EXPECT_TRUE(connected(rp.output(), r0.right()));
    EXPECT_TRUE(connected(r0.output(), ro.input()));
    EXPECT_TRUE(same(variables(rp.columns()), { x0 }));
}

// Right columns referred only by the join condition are used to find matches.
// The join, its right input, and those columns stay.
TEST_F(remove_unused_stream_variables_contract_test, right_columns_used_only_by_condition_are_kept) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(c0, d1),
    });
    auto&& ro = r.insert(relation::emit { c2 });
    rl.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 4);
    EXPECT_TRUE(connected(rr.output(), r0.right()));
    EXPECT_TRUE(same(destinations(rr.columns()), { d1 }));
}

// The condition of a filter selects rows, so columns in it count as referred.
TEST_F(remove_unused_stream_variables_contract_test, filter_uses_columns) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::filter { compare(c0, c1) });
    auto&& ro = r.insert(relation::emit { c2 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 3);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1, c2 }));
}

// A project keeps only the columns referred downstream or by a later column of the same project,
// because later columns are computed from earlier ones. A project with no columns left is removed.
TEST_F(remove_unused_stream_variables_contract_test, project_keeps_columns_referred_by_later_columns) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::project {
            relation::project::column { scalar::variable_reference { c0 }, x0 },
            relation::project::column { scalar::variable_reference { c1 }, x1 },
            relation::project::column { scalar::variable_reference { x1 }, x2 },
    });
    auto&& ro = r.insert(relation::emit { x2 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(variables(r1.columns()), { x1, x2 }));
    EXPECT_TRUE(same(destinations(r0.columns()), { c1 }));
}

TEST_F(remove_unused_stream_variables_contract_test, project_is_removed_with_its_last_column) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::project {
            relation::project::column { scalar::variable_reference { c0 }, x0 },
    });
    auto&& ro = r.insert(relation::emit { c1 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 2);
    EXPECT_TRUE(connected(r0.output(), ro.input()));
    EXPECT_TRUE(same(destinations(r0.columns()), { c1 }));
}

// A column stays if any branch after a buffer refers to it. It is removed only if no branch refers to it.
TEST_F(remove_unused_stream_variables_contract_test, buffer_keeps_columns_of_any_branch) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::buffer { 3 });
    auto&& ro0 = r.insert(relation::emit { c0 });
    auto&& ro1 = r.insert(relation::emit { c1 });
    auto&& ro2 = r.insert(no_columns());
    r0.output() >> r1.input();
    r1.output_ports()[0] >> ro0.input();
    r1.output_ports()[1] >> ro1.input();
    r1.output_ports()[2] >> ro2.input();

    apply();

    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1 }));
}

// A project or an identify before a buffer follows the same rule: references from all branches count.
TEST_F(remove_unused_stream_variables_contract_test, project_before_buffer) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::project {
            relation::project::column { scalar::variable_reference { c0 }, x0 },
            relation::project::column { scalar::variable_reference { c1 }, x1 },
            relation::project::column { scalar::variable_reference { c2 }, x2 },
    });
    auto&& r2 = r.insert(relation::buffer { 2 });
    auto&& ro0 = r.insert(relation::emit { x0 });
    auto&& ro1 = r.insert(relation::emit { x1 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output_ports()[0] >> ro0.input();
    r2.output_ports()[1] >> ro1.input();

    apply();

    EXPECT_TRUE(same(variables(r1.columns()), { x0, x1 }));
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1 }));
}

TEST_F(remove_unused_stream_variables_contract_test, identify_before_buffer_is_kept_if_a_branch_refers) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::identify { x0, t::row_id { 2 } });
    auto&& r2 = r.insert(relation::buffer { 2 });
    auto&& ro0 = r.insert(relation::emit { x0 });
    auto&& ro1 = r.insert(relation::emit { c1 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output_ports()[0] >> ro0.input();
    r2.output_ports()[1] >> ro1.input();

    apply();

    ASSERT_EQ(r.size(), 5);
    EXPECT_TRUE(connected(r1.output(), r2.input()));
}

TEST_F(remove_unused_stream_variables_contract_test, identify_before_buffer_is_removed_if_no_branch_refers) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::identify { x0, t::row_id { 2 } });
    auto&& r2 = r.insert(relation::buffer { 2 });
    auto&& ro0 = r.insert(relation::emit { c0 });
    auto&& ro1 = r.insert(relation::emit { c1 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output_ports()[0] >> ro0.input();
    r2.output_ports()[1] >> ro1.input();

    apply();

    ASSERT_EQ(r.size(), 4);
    EXPECT_TRUE(connected(r0.output(), r2.input()));
}

// An identify stays if its column is referred downstream. Otherwise, it is removed.
TEST_F(remove_unused_stream_variables_contract_test, identify_is_kept_if_referred) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::identify { x0, t::row_id { 2 } });
    auto&& ro = r.insert(relation::emit { x0 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 3);
    EXPECT_TRUE(connected(r1.output(), ro.input()));
}

TEST_F(remove_unused_stream_variables_contract_test, identify_is_removed_if_not_referred) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::identify { x0, t::row_id { 2 } });
    auto&& ro = r.insert(relation::emit { c0 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 2);
    EXPECT_TRUE(connected(r0.output(), ro.input()));
}

// A removed operator is replaced by a connection from its upstream output to its downstream input,
// if both are connected. The input port stays the same, because the left and the right inputs of some operators differ.
TEST_F(remove_unused_stream_variables_contract_test, removed_operator_on_the_right_input_keeps_the_side) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& rp = r.insert(relation::project {
            relation::project::column { scalar::variable_reference { d2 }, x0 },
    });
    auto&& r0 = r.insert(relation::intermediate::join {
            relation::join_kind::left_outer,
            compare(c0, d0),
    });
    auto&& ro = r.insert(relation::emit { c1 });
    rl.output() >> r0.left();
    rr.output() >> rp.input();
    rp.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 4);
    EXPECT_TRUE(connected(rl.output(), r0.left()));
    EXPECT_TRUE(connected(rr.output(), r0.right()));
}

TEST_F(remove_unused_stream_variables_contract_test, removed_operator_on_the_left_input_keeps_the_side) {
    auto&& rl = left_scan();
    auto&& rr = right_scan();
    auto&& rp = r.insert(relation::project {
            relation::project::column { scalar::variable_reference { c2 }, x0 },
    });
    auto&& r0 = r.insert(relation::intermediate::difference {
            { { c0, d0 } },
            relation::set_quantifier::distinct,
    });
    auto&& ro = r.insert(relation::emit { c0 });
    rl.output() >> rp.input();
    rp.output() >> r0.left();
    rr.output() >> r0.right();
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 4);
    EXPECT_TRUE(connected(rl.output(), r0.left()));
    EXPECT_TRUE(connected(rr.output(), r0.right()));
}

TEST_F(remove_unused_stream_variables_contract_test, chain_of_removed_operators) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::project {
            relation::project::column { scalar::variable_reference { c0 }, x0 },
    });
    auto&& r2 = r.insert(relation::project {
            relation::project::column { scalar::variable_reference { x0 }, x1 },
    });
    auto&& ro = r.insert(relation::emit { c1 });
    r0.output() >> r1.input();
    r1.output() >> r2.input();
    r2.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 2);
    EXPECT_TRUE(connected(r0.output(), ro.input()));
    EXPECT_TRUE(same(destinations(r0.columns()), { c1 }));
}

// Unused columns of scan and find are removed, but the operators stay.
// The number of rows does not depend on the columns.
TEST_F(remove_unused_stream_variables_contract_test, scan_is_kept_without_columns) {
    auto&& r0 = left_scan();
    auto&& ro = r.insert(no_columns());
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 2);
    EXPECT_TRUE(connected(r0.output(), ro.input()));
    EXPECT_TRUE(r0.columns().empty());
}

TEST_F(remove_unused_stream_variables_contract_test, find_is_kept_without_columns) {
    auto&& r0 = r.insert(relation::find {
            bindings(*i0),
            {
                    { t0c0, c0 },
                    { t0c1, c1 },
            },
            {
                    relation::find::key { t0c0, constant() },
            },
    });
    auto&& ro = r.insert(no_columns());
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 2);
    EXPECT_TRUE(connected(r0.output(), ro.input()));
    EXPECT_TRUE(r0.columns().empty());
    EXPECT_EQ(r0.keys().size(), 1);
}

// Unused columns of values are removed with their row elements. All rows stay.
// The number of rows does not depend on the columns.
TEST_F(remove_unused_stream_variables_contract_test, values_keeps_rows_without_columns) {
    auto&& r0 = r.insert(relation::values {
            { c0, c1 },
            {
                    { constant(0), constant(1) },
                    { constant(2), constant(3) },
                    { constant(4), constant(5) },
            },
    });
    auto&& ro = r.insert(no_columns());
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r.size(), 2);
    EXPECT_TRUE(r0.columns().empty());
    ASSERT_EQ(r0.rows().size(), 3);
    for (auto&& row : r0.rows()) {
        EXPECT_TRUE(row.elements().empty());
    }
}

TEST_F(remove_unused_stream_variables_contract_test, values_keeps_elements_of_remaining_columns) {
    auto&& r0 = r.insert(relation::values {
            { c0, c1, c2 },
            {
                    { constant(0), constant(1), constant(2) },
                    { constant(3), constant(4), constant(5) },
            },
    });
    auto&& ro = r.insert(relation::emit { c1 });
    r0.output() >> ro.input();

    apply();

    ASSERT_EQ(r0.columns().size(), 1);
    EXPECT_EQ(r0.columns()[0], c1);
    ASSERT_EQ(r0.rows().size(), 2);
    ASSERT_EQ(r0.rows()[0].elements().size(), 1);
    EXPECT_EQ(r0.rows()[0].elements()[0], constant(1));
    ASSERT_EQ(r0.rows()[1].elements().size(), 1);
    EXPECT_EQ(r0.rows()[1].elements()[0], constant(4));
}

// Columns in the arguments of apply count as referred. Unused output columns are removed.
// The apply stays, because the number of rows depends on the function.
TEST_F(remove_unused_stream_variables_contract_test, apply_is_kept_and_uses_arguments) {
    auto&& r0 = left_scan();
    auto&& tvf = bindings.function({
            function::declaration::minimum_user_function_id + 1,
            "tvf",
            ::takatori::type::table {
                    { "x1", ::takatori::type::int8 {} },
                    { "x2", ::takatori::type::int8 {} },
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
                    scalar::variable_reference { c1 },
            },
            {
                    { 0, x0 },
                    { 1, x1 },
            },
    });
    auto&& ro = r.insert(relation::emit { c2 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    // the number of rows depends on the function
    ASSERT_EQ(r.size(), 3);
    EXPECT_TRUE(connected(r0.output(), r1.input()));
    EXPECT_TRUE(r1.columns().empty());
    EXPECT_TRUE(same(destinations(r0.columns()), { c1, c2 }));
}

// Unused mappings of escape are removed. The sources of the remaining mappings count as referred.
TEST_F(remove_unused_stream_variables_contract_test, escape_uses_only_remaining_sources) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::intermediate::escape {
            { c0, x0 },
            { c1, x1 },
    });
    auto&& ro = r.insert(relation::emit { x1 });
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(destinations(r1.mappings()), { x1 }));
    EXPECT_TRUE(same(destinations(r0.columns()), { c1 }));
}

// The sources of the columns of emit and write count as referred, and so do the keys of write,
// because they decide the result and the written values.
TEST_F(remove_unused_stream_variables_contract_test, emit_uses_sources) {
    auto&& r0 = left_scan();
    auto&& ro = r.insert(relation::emit { c0, c2 });
    r0.output() >> ro.input();

    apply();

    EXPECT_EQ(ro.columns().size(), 2);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c2 }));
}

TEST_F(remove_unused_stream_variables_contract_test, write_uses_keys_and_columns) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::write {
            relation::write_kind::update,
            bindings(*i1),
            {
                    { c0, t1c0 },
            },
            {
                    { c2, t1c2 },
            },
    });
    r0.output() >> r1.input();

    apply();

    EXPECT_EQ(r1.keys().size(), 1);
    EXPECT_EQ(r1.columns().size(), 1);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c2 }));
}

// join_find and join_scan check their conditions before removing columns.
// A column referred only by the condition of the same operator stays.
TEST_F(remove_unused_stream_variables_contract_test, join_find_keeps_columns_used_by_condition) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::join_find {
            relation::join_kind::left_outer,
            bindings(*i1),
            {
                    { t1c0, d0 },
                    { t1c1, d1 },
                    { t1c2, d2 },
            },
            {
                    relation::join_find::key { t1c0, scalar::variable_reference { c0 } },
            },
            compare(c1, d1),
    });
    auto&& ro = r.insert(relation::emit { c2 });
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(destinations(r1.columns()), { d1 }));
    EXPECT_EQ(r1.keys().size(), 1);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1, c2 }));
}

TEST_F(remove_unused_stream_variables_contract_test, join_scan_keeps_columns_used_by_condition) {
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::join_scan {
            relation::join_kind::left_outer,
            bindings(*i1),
            {
                    { t1c0, d0 },
                    { t1c1, d1 },
                    { t1c2, d2 },
            },
            {
                    relation::join_scan::key { t1c0, scalar::variable_reference { c0 } },
                    relation::endpoint_kind::inclusive,
            },
            {
                    relation::join_scan::key { t1c0, scalar::variable_reference { c2 } },
                    relation::endpoint_kind::inclusive,
            },
            compare(c1, d1),
    });
    auto&& ro = r.insert(no_columns());
    r0.output() >> r1.left();
    r1.output() >> ro.input();

    apply();

    EXPECT_TRUE(same(destinations(r1.columns()), { d1 }));
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c1, c2 }));
}

// Only the body of let decides its value. Unused declarators are removed after checking the body.
// A declarator referred by a later declarator is used.
TEST_F(remove_unused_stream_variables_contract_test, let_removes_declarators_after_the_body) {
    auto l0 = bindings.local_variable("l0");
    auto l1 = bindings.local_variable("l1");
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::filter {
            scalar::let {
                    {
                            scalar::let::variable { l0, scalar::variable_reference { c0 } },
                            scalar::let::variable { l1, scalar::variable_reference { c1 } },
                    },
                    compare(l1, c2),
            },
    });
    auto&& ro = r.insert(no_columns());
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    auto&& let = unsafe_downcast<scalar::let>(r1.condition());
    ASSERT_EQ(let.variables().size(), 1);
    EXPECT_EQ(let.variables()[0].variable(), l1);
    EXPECT_TRUE(same(destinations(r0.columns()), { c1, c2 }));
}

TEST_F(remove_unused_stream_variables_contract_test, let_keeps_declarators_referred_by_later_ones) {
    auto l0 = bindings.local_variable("l0");
    auto l1 = bindings.local_variable("l1");
    auto&& r0 = left_scan();
    auto&& r1 = r.insert(relation::filter {
            scalar::let {
                    {
                            scalar::let::variable { l0, scalar::variable_reference { c0 } },
                            scalar::let::variable { l1, scalar::variable_reference { l0 } },
                    },
                    compare(l1, c2),
            },
    });
    auto&& ro = r.insert(no_columns());
    r0.output() >> r1.input();
    r1.output() >> ro.input();

    apply();

    auto&& let = unsafe_downcast<scalar::let>(r1.condition());
    EXPECT_EQ(let.variables().size(), 2);
    EXPECT_TRUE(same(destinations(r0.columns()), { c0, c2 }));
}

} // namespace yugawara::analyzer::details
