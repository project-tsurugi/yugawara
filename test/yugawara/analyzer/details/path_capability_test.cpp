#include <yugawara/analyzer/details/path_capability.h>

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
#include <takatori/relation/join_find.h>
#include <takatori/relation/join_scan.h>

#include <takatori/relation/intermediate/escape.h>
#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/limit.h>
#include <takatori/relation/intermediate/union.h>

#include <yugawara/binding/factory.h>
#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class path_capability_test : public ::testing::Test {
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

    descriptor::variable c0 = bindings.stream_variable("c0");
    descriptor::variable c1 = bindings.stream_variable("c1");

    relation::scan& add_scan(relation::graph_type& r) {
        return r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, c0 },
                        { t0c1, c1 },
                },
        });
    }

    relation::intermediate::join& add_join(relation::graph_type& r, relation::join_kind kind) {
        auto&& r0 = add_scan(r);
        auto&& r1 = r.insert(relation::scan {
                bindings(*i0),
                {
                        { t0c0, bindings.stream_variable("d0") },
                        { t0c1, bindings.stream_variable("d1") },
                },
        });
        auto&& r2 = r.insert(relation::intermediate::join { kind });
        r0.output() >> r2.left();
        r1.output() >> r2.right();
        return r2;
    }

    relation::join_find& add_join_find(relation::graph_type& r, relation::join_kind kind) {
        auto&& r0 = add_scan(r);
        auto&& r1 = r.insert(relation::join_find {
                kind,
                bindings(*i0),
                {
                        { t0c0, bindings.stream_variable("d0") },
                },
                {},
        });
        r0.output() >> r1.left();
        return r1;
    }

    relation::join_scan& add_join_scan(relation::graph_type& r, relation::join_kind kind) {
        auto&& r0 = add_scan(r);
        auto&& r1 = r.insert(relation::join_scan {
                kind,
                bindings(*i0),
                {
                        { t0c0, bindings.stream_variable("d0") },
                },
                {},
                {},
        });
        r0.output() >> r1.left();
        return r1;
    }

    relation::apply& add_apply(relation::graph_type& r, relation::apply_kind kind) {
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
        auto&& r1 = r.insert(relation::apply {
                tvf,
                {
                        varref { c0 },
                },
                {
                        { 0, bindings.stream_variable("x0") },
                },
                kind,
        });
        r0.output() >> r1.input();
        return r1;
    }
};

TEST_F(path_capability_test, project) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    varref { c0 },
                    bindings.stream_variable("x"),
            },
    });
    r0.output() >> r1.input();

    auto result = find_path_capabilities(r1.input(), r1.output());
    EXPECT_EQ(result, (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, filter) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::filter {
            compare(c0, c1),
    });
    r0.output() >> r1.input();

    auto result = find_path_capabilities(r1.input(), r1.output());
    EXPECT_EQ(result, (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, limit) {
    // NOTE: limits are the boundaries of the paths, even if they only drop rows
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::limit {
            relation::intermediate::limit::row_slice_type { {}, 3 },
    });
    r0.output() >> r1.input();

    auto result = find_path_capabilities(r1.input(), r1.output());
    EXPECT_EQ(result, path_capability_set {});
}

TEST_F(path_capability_test, limit_sort_keys) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::limit {
            {},
            { c0 },
            relation::intermediate::limit::row_slice_type { {}, 3 },
    });
    r0.output() >> r1.input();

    auto result = find_path_capabilities(r1.input(), r1.output());
    EXPECT_EQ(result, path_capability_set {});
}

TEST_F(path_capability_test, escape) {
    // NOTE: escape is the boundary of the paths, even if it only renames the columns
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::intermediate::escape {
            { c0, bindings.stream_variable("x0") },
            { c1, bindings.stream_variable("x1") },
    });
    r0.output() >> r1.input();

    auto result = find_path_capabilities(r1.input(), r1.output());
    EXPECT_EQ(result, path_capability_set {});
}

TEST_F(path_capability_test, apply_cross) {
    // NOTE: cross apply drops the input rows if the function returns no rows
    relation::graph_type r;
    auto&& r0 = add_apply(r, relation::apply_kind::cross);

    auto result = find_path_capabilities(r0.input(), r0.output());
    EXPECT_EQ(result, path_capability_set {});
}

TEST_F(path_capability_test, apply_outer) {
    // NOTE: the row order depends on the implementation of table-valued functions
    relation::graph_type r;
    auto&& r0 = add_apply(r, relation::apply_kind::outer);

    auto result = find_path_capabilities(r0.input(), r0.output());
    EXPECT_EQ(result, (path_capability_set {
            path_capability::at_least_one,
    }));
}

TEST_F(path_capability_test, different_operators) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::project {
            relation::project::column {
                    varref { c0 },
                    bindings.stream_variable("x"),
            },
    });
    auto&& r2 = r.insert(relation::project {
            relation::project::column {
                    varref { c1 },
                    bindings.stream_variable("y"),
            },
    });
    r0.output() >> r1.input();
    r1.output() >> r2.input();

    auto result = find_path_capabilities(r1.input(), r2.output());
    EXPECT_EQ(result, path_capability_set {});
}

TEST_F(path_capability_test, buffer_stop_with_primary) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::buffer { 2, true });
    r0.output() >> r1.input();

    EXPECT_EQ(find_path_capabilities(r1.input(), r1.primary_output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::at_most_one,
    }));
    EXPECT_EQ(find_path_capabilities(r1.input(), r1.secondary_outputs()[0]), path_capability_set {});
}

TEST_F(path_capability_test, buffer_without_stop_with_primary) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = r.insert(relation::buffer { 2 });
    r0.output() >> r1.input();

    EXPECT_EQ(find_path_capabilities(r1.input(), r1.primary_output()), path_capability_set {});
    EXPECT_EQ(find_path_capabilities(r1.input(), r1.secondary_outputs()[0]), path_capability_set {});
}

TEST_F(path_capability_test, join_inner) {
    relation::graph_type r;
    auto&& j = add_join(r, relation::join_kind::inner);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), path_capability_set {});
    EXPECT_EQ(find_path_capabilities(j.right(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, join_left_outer) {
    relation::graph_type r;
    auto&& j = add_join(r, relation::join_kind::left_outer);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::at_least_one,
    }));
    EXPECT_EQ(find_path_capabilities(j.right(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, join_left_outer_at_most_one) {
    relation::graph_type r;
    auto&& j = add_join(r, relation::join_kind::left_outer_at_most_one);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::at_least_one,
            path_capability::at_most_one,
    }));
    EXPECT_EQ(find_path_capabilities(j.right(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, join_find_inner) {
    relation::graph_type r;
    auto&& j = add_join_find(r, relation::join_kind::inner);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
    }));
}

TEST_F(path_capability_test, join_find_left_outer) {
    relation::graph_type r;
    auto&& j = add_join_find(r, relation::join_kind::left_outer);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_least_one,
    }));
}

TEST_F(path_capability_test, join_find_left_outer_at_most_one) {
    relation::graph_type r;
    auto&& j = add_join_find(r, relation::join_kind::left_outer_at_most_one);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, join_find_full_outer) {
    relation::graph_type r;
    auto&& j = add_join_find(r, relation::join_kind::full_outer);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, join_find_semi) {
    relation::graph_type r;
    auto&& j = add_join_find(r, relation::join_kind::semi);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, join_find_anti) {
    relation::graph_type r;
    auto&& j = add_join_find(r, relation::join_kind::anti);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, join_scan_inner) {
    relation::graph_type r;
    auto&& j = add_join_scan(r, relation::join_kind::inner);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
    }));
}

TEST_F(path_capability_test, join_scan_left_outer) {
    relation::graph_type r;
    auto&& j = add_join_scan(r, relation::join_kind::left_outer);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_least_one,
    }));
}

TEST_F(path_capability_test, join_scan_left_outer_at_most_one) {
    relation::graph_type r;
    auto&& j = add_join_scan(r, relation::join_kind::left_outer_at_most_one);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_least_one,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, join_scan_full_outer) {
    relation::graph_type r;
    auto&& j = add_join_scan(r, relation::join_kind::full_outer);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, join_scan_semi) {
    relation::graph_type r;
    auto&& j = add_join_scan(r, relation::join_kind::semi);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, join_scan_anti) {
    relation::graph_type r;
    auto&& j = add_join_scan(r, relation::join_kind::anti);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::preserves_order,
            path_capability::at_most_one,
    }));
}

TEST_F(path_capability_test, join_full_outer) {
    relation::graph_type r;
    auto&& j = add_join(r, relation::join_kind::full_outer);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), path_capability_set {});
    EXPECT_EQ(find_path_capabilities(j.right(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, join_semi) {
    relation::graph_type r;
    auto&& j = add_join(r, relation::join_kind::semi);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::at_most_one,
    }));
    EXPECT_EQ(find_path_capabilities(j.right(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, join_anti) {
    relation::graph_type r;
    auto&& j = add_join(r, relation::join_kind::anti);

    EXPECT_EQ(find_path_capabilities(j.left(), j.output()), (path_capability_set {
            path_capability::at_most_one,
    }));
    EXPECT_EQ(find_path_capabilities(j.right(), j.output()), path_capability_set {});
}

TEST_F(path_capability_test, union_all) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan(r);
    auto&& u = r.insert(relation::intermediate::union_ {
            {
                    { c0, c0, bindings.stream_variable("u0") },
            },
            relation::set_quantifier::all,
    });
    r0.output() >> u.left();
    r1.output() >> u.right();

    EXPECT_EQ(find_path_capabilities(u.left(), u.output()), (path_capability_set {
            path_capability::at_least_one,
    }));
    EXPECT_EQ(find_path_capabilities(u.right(), u.output()), (path_capability_set {
            path_capability::at_least_one,
    }));
}

TEST_F(path_capability_test, union_distinct) {
    relation::graph_type r;
    auto&& r0 = add_scan(r);
    auto&& r1 = add_scan(r);
    auto&& u = r.insert(relation::intermediate::union_ {
            {
                    { c0, c0, bindings.stream_variable("u0") },
            },
            relation::set_quantifier::distinct,
    });
    r0.output() >> u.left();
    r1.output() >> u.right();

    EXPECT_EQ(find_path_capabilities(u.left(), u.output()), path_capability_set {});
    EXPECT_EQ(find_path_capabilities(u.right(), u.output()), path_capability_set {});
}

} // namespace yugawara::analyzer::details
