#include <yugawara/analyzer/details/collect_local_variables.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/unary.h>
#include <takatori/scalar/cast.h>
#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>
#include <takatori/scalar/match.h>
#include <takatori/scalar/conditional.h>
#include <takatori/scalar/coalesce.h>
#include <takatori/scalar/let.h>
#include <takatori/scalar/function_call.h>

#include <takatori/relation/find.h>
#include <takatori/relation/scan.h>
#include <takatori/relation/values.h>
#include <takatori/relation/project.h>
#include <takatori/relation/filter.h>

#include <takatori/relation/intermediate/join.h>

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>

#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class collect_local_variables_test : public ::testing::Test {
protected:

    scalar::variable_reference var(descriptor::variable v) {
        return scalar::variable_reference {
                std::move(v),
        };
    }

    scalar::immediate val(int v = 0) {
        return scalar::immediate {
                v::int4 { v },
                t::int4 {},
        };
    }

    scalar::immediate replacement(int v) {
        return val(-v);
    }
    
    scalar::let declaration(int v) {
        auto vv = bindings.stream_variable();
        return scalar::let {
                scalar::let::declarator { vv, replacement(v) },
                var(vv),
        };
    }

    std::unique_ptr<scalar::expression> apply(scalar::expression&& expr) const {
        auto target = ::takatori::util::clone_unique(std::move(expr));
        collect_local_variables(target, always_inline);
        return target;
    }

    std::unique_ptr<relation::expression> apply(relation::expression&& expr) const {
        ::takatori::relation::graph_type graph;
        auto&& r = graph.insert(::takatori::util::clone_unique(std::move(expr)));
        collect_local_variables(graph, always_inline);
        if (graph.size() == 1) {
            return graph.release(r);
        }
        return ::takatori::util::clone_unique(relation::values {
                {},
                {},
        });
    }

    binding::factory bindings;
    bool always_inline = true;


    storage::configurable_provider storages;
    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    std::shared_ptr<storage::index> i0 = storages.add_index({ t0, "I0", });
};

TEST_F(collect_local_variables_test, let_simple) {
    auto v = bindings.stream_variable();
    auto r = apply(scalar::let {
            scalar::let::declarator { v, val() },
            var(v),
    });
    EXPECT_EQ(*r, val());
}

TEST_F(collect_local_variables_test, let_multiple) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto v3 = bindings.stream_variable();
    auto r = apply(scalar::let {
            {
                    scalar::let::declarator { v1, val() },
                    scalar::let::declarator { v2, var(v1) },
                    scalar::let::declarator { v3, var(v2) },
            },
            var(v3),
    });
    EXPECT_EQ(*r, val());
}

TEST_F(collect_local_variables_test, let_nesting_in_declarator) {
    auto v = bindings.stream_variable();
    auto r = apply(scalar::let {
            scalar::let::declarator {
                    v,
                    declaration(10),
            },
            var(v),
    });
    EXPECT_EQ(*r, replacement(10));
}

TEST_F(collect_local_variables_test, let_nesting_in_body) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto r = apply(scalar::let {
            scalar::let::declarator { v1, val() },
            scalar::let {
                    scalar::let::declarator { v2, var(v1) },
                    var(v2),
            },
    });
    EXPECT_EQ(*r, val());
}

TEST_F(collect_local_variables_test, immediate) {
    auto r = apply(scalar::immediate {
            v::int4 { 100 },
            t::int4 {},
    });
    EXPECT_EQ(*r, (scalar::immediate {
            v::int4 { 100 },
            t::int4 {},
    }));
}

TEST_F(collect_local_variables_test, variable_reference) {
    auto v = bindings.stream_variable();
    auto r = apply(scalar::variable_reference {
            v,
    });
    EXPECT_EQ(*r, (scalar::variable_reference {
            v,
    }));
}

TEST_F(collect_local_variables_test, unary) {
    auto r = apply(scalar::unary {
            scalar::unary_operator::plus,
            declaration(10),
    });
    EXPECT_EQ(*r, (scalar::unary {
            scalar::unary_operator::plus,
            replacement(10),
    }));
}

TEST_F(collect_local_variables_test, cast) {
    auto r = apply(scalar::cast {
            t::int8 {},
            scalar::cast_loss_policy::error,
            declaration(10),
    });
    EXPECT_EQ(*r, (scalar::cast {
            t::int8 {},
            scalar::cast_loss_policy::error,
            replacement(10),
    }));
}

TEST_F(collect_local_variables_test, binary) {
    auto r = apply(scalar::binary {
            scalar::binary_operator::add,
            declaration(10),
            declaration(20),
    });
    EXPECT_EQ(*r, (scalar::binary {
            scalar::binary_operator::add,
            replacement(10),
            replacement(20),
    }));
}

TEST_F(collect_local_variables_test, compare) {
    auto r = apply(scalar::compare {
            scalar::comparison_operator::equal,
            declaration(10),
            declaration(20),
    });
    EXPECT_EQ(*r, (scalar::compare {
            scalar::comparison_operator::equal,
            replacement(10),
            replacement(20),
    }));
}

TEST_F(collect_local_variables_test, match) {
    auto r = apply(scalar::match {
            scalar::match_operator::like,
            declaration(10),
            declaration(20),
            declaration(30),
    });
    EXPECT_EQ(*r, (scalar::match {
            scalar::match_operator::like,
            replacement(10),
            replacement(20),
            replacement(30),
    }));
}

TEST_F(collect_local_variables_test, conditional_hit) {
    auto r = apply(scalar::conditional {
            {
                    scalar::conditional::alternative { declaration(10), declaration(20) },
                    scalar::conditional::alternative { declaration(30), declaration(40) },
            },
            declaration(50),
    });
    EXPECT_EQ(*r, (scalar::conditional {
            {
                    scalar::conditional::alternative { replacement(10), replacement(20), },
                    scalar::conditional::alternative { replacement(30), replacement(40), },
            },
            replacement(50),
    }));
}

TEST_F(collect_local_variables_test, coalesce_hit) {
    auto r = apply(scalar::coalesce {
            {
                    replacement(10),
                    replacement(20),
                    replacement(30),
            }
    });
    EXPECT_EQ(*r, (scalar::coalesce {
            {
                    replacement(10),
                    replacement(20),
                    replacement(30),
            }
    }));
}

TEST_F(collect_local_variables_test, function_call) {
    auto f = bindings.function({
            1,
            "f",
            t::int4 {},
            {
                    t::int4 {},
                    t::int4 {},
                    t::int4 {},
            },
    });
    auto r = apply(scalar::function_call {
            f,
            {
                    replacement(10),
                    replacement(20),
                    replacement(30),
            },
    });
    EXPECT_EQ(*r, (scalar::function_call {
            f,
            {
                    replacement(10),
                    replacement(20),
                    replacement(30),
            },
    }));
}

TEST_F(collect_local_variables_test, find) {
    auto r = apply(relation::find {
            bindings(i0),
            {},
            {
                    relation::find::key { bindings(t0->columns()[0]), declaration(10) },
            },
    });
    auto&& f = downcast<relation::find>(*r);

    ASSERT_EQ(f.keys().size(), 1);
    EXPECT_EQ(f.keys()[0].value(), replacement(10));
}

TEST_F(collect_local_variables_test, scan) {
    auto r = apply(relation::scan {
            bindings(i0),
            {},
            {
                    relation::scan::key { bindings(t0->columns()[0]), declaration(10) },
                    relation::endpoint_kind::inclusive,
            },
            {
                    relation::scan::key { bindings(t0->columns()[0]), declaration(20) },
                    relation::endpoint_kind::inclusive,
            },
            {},
    });
    auto&& f = downcast<relation::scan>(*r);

    ASSERT_EQ(f.lower().keys().size(), 1);
    EXPECT_EQ(f.lower().keys()[0].value(), replacement(10));

    ASSERT_EQ(f.upper().keys().size(), 1);
    EXPECT_EQ(f.upper().keys()[0].value(), replacement(20));
}

TEST_F(collect_local_variables_test, join_relation) {
    auto r = apply(relation::intermediate::join {
            relation::join_kind::inner,
            {
                    relation::intermediate::join::key { bindings(t0->columns()[0]), declaration(10) },
                    relation::endpoint_kind::inclusive,
            },
            {
                    relation::intermediate::join::key { bindings(t0->columns()[0]), declaration(20) },
                    relation::endpoint_kind::inclusive,
            },
            ::takatori::util::clone_unique(declaration(30)),
    });
    auto&& f = downcast<relation::intermediate::join>(*r);

    ASSERT_EQ(f.lower().keys().size(), 1);
    EXPECT_EQ(f.lower().keys()[0].value(), replacement(10));

    ASSERT_EQ(f.upper().keys().size(), 1);
    EXPECT_EQ(f.upper().keys()[0].value(), replacement(20));

    EXPECT_EQ(f.condition(), replacement(30));
}

TEST_F(collect_local_variables_test, project) {
    auto r = apply(relation::project {
            relation::project::column { bindings.stream_variable(), declaration(10) },
            relation::project::column { bindings.stream_variable(), declaration(20) },
            relation::project::column { bindings.stream_variable(), declaration(30) },
    });
    auto&& f = downcast<relation::project>(*r);

    ASSERT_EQ(f.columns().size(), 3);
    EXPECT_EQ(f.columns()[0].value(), replacement(10));
    EXPECT_EQ(f.columns()[1].value(), replacement(20));
    EXPECT_EQ(f.columns()[2].value(), replacement(30));
}

TEST_F(collect_local_variables_test, filter) {
    auto r = apply(relation::filter {
            declaration(10),
    });
    auto&& f = downcast<relation::filter>(*r);

    EXPECT_EQ(f.condition(), replacement(10));
}

} // namespace yugawara::analyzer::details
