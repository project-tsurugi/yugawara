#include <yugawara/extension/relation/subquery.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/value/primitive.h>

#include <takatori/scalar/immediate.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>

#include <takatori/util/clonable.h>

#include <yugawara/binding/factory.h>

namespace yugawara::extension::relation {

class relation_subquery_test : public ::testing::Test {
public:
    binding::factory bindings;
};

static_assert(subquery::tag == ::takatori::relation::intermediate::extension::tag);
static_assert(subquery::extension_tag == extension_id::subquery_id);

using ::takatori::util::clone_unique;

static ::takatori::scalar::immediate constant(int v) {
    return ::takatori::scalar::immediate {
        ::takatori::value::int4 { v },
        ::takatori::type::int4  {},
};
}

TEST_F(relation_subquery_test, simple) {
    auto i0 = bindings.stream_variable("i0");
    auto o0 = bindings.stream_variable("o0");

    ::takatori::relation::graph_type graph;
    graph.insert(::takatori::relation::values {
            {
                    i0,
            },
            {
                    {
                            constant(1),
                    },
            },
    });
    subquery expr {
            std::move(graph),
            {
                    { i0, o0 },
            },
    };

    EXPECT_EQ(expr.query_graph().size(), 1);

    auto output = expr.find_output_port();
    ASSERT_TRUE(output);
    EXPECT_TRUE(expr.query_graph().contains(output->owner()));

    ASSERT_EQ(expr.mappings().size(), 1);
    EXPECT_EQ(expr.mappings()[0].source(), i0);
    EXPECT_EQ(expr.mappings()[0].destination(), o0);
}

TEST_F(relation_subquery_test, clone) {
    auto i0 = bindings.stream_variable("i0");
    auto o0 = bindings.stream_variable("o0");

    ::takatori::relation::graph_type graph;
    graph.insert(::takatori::relation::values {
            {
                    i0,
            },
            {
                    {
                            constant(1),
                    },
            },
    });
    subquery expr {
            std::move(graph),
            {
                    { i0, o0 },
            },
    };

    auto copy = clone_unique(expr);
    EXPECT_NE(std::addressof(expr), copy.get());

    EXPECT_EQ(copy->query_graph().size(), 1);

    auto output = copy->find_output_port();
    ASSERT_TRUE(output);
    EXPECT_EQ(output->owner().kind(), ::takatori::relation::values::tag);
    EXPECT_TRUE(copy->query_graph().contains(output->owner()));
    EXPECT_FALSE(expr.query_graph().contains(output->owner()));

    ASSERT_EQ(copy->mappings().size(), 1);
    EXPECT_EQ(expr.mappings(), copy->mappings());

    EXPECT_TRUE(copy->is_clone());
}

TEST_F(relation_subquery_test, output) {
    auto i0 = bindings.stream_variable("i0");
    auto o0 = bindings.stream_variable("o0");

    ::takatori::relation::graph_type graph;
    graph.insert(::takatori::relation::values {
            {
                    i0,
            },
            {
                    {
                            constant(1),
                    },
            },
    });
    subquery expr {
            std::move(graph),
            {
                    { i0, o0 },
            },
    };
    std::cout << expr << std::endl;
}

} // namespace yugawara::extension::relation
