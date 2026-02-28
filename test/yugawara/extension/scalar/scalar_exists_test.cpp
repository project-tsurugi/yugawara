#include <yugawara/extension/scalar/exists.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/value/primitive.h>

#include <takatori/scalar/immediate.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>

#include <takatori/util/clonable.h>

#include <yugawara/binding/factory.h>

namespace yugawara::extension::scalar {

class scalar_exists_test : public ::testing::Test {
public:
    binding::factory bindings;
};

static_assert(exists::tag == ::takatori::scalar::extension::tag);
static_assert(exists::extension_tag == exists_id);

using ::takatori::util::clone_unique;

static ::takatori::scalar::immediate constant(int v) {
    return ::takatori::scalar::immediate {
        ::takatori::value::int4 { v },
        ::takatori::type::int4  {},
};
}

TEST_F(scalar_exists_test, simple) {
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
    exists expr {
            std::move(graph),
    };

    EXPECT_EQ(expr.query_graph().size(), 1);

    auto output = expr.find_output_port();
    ASSERT_TRUE(output);
    EXPECT_TRUE(expr.query_graph().contains(output->owner()));
}

TEST_F(scalar_exists_test, clone) {
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
    exists expr {
            std::move(graph),
    };

    auto copy = clone_unique(expr);
    EXPECT_NE(std::addressof(expr), copy.get());

    EXPECT_EQ(copy->query_graph().size(), 1);

    auto output = copy->find_output_port();
    ASSERT_TRUE(output);
    EXPECT_EQ(output->owner().kind(), ::takatori::relation::values::tag);
    EXPECT_TRUE(copy->query_graph().contains(output->owner()));
    EXPECT_FALSE(expr.query_graph().contains(output->owner()));
}

TEST_F(scalar_exists_test, output) {
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
    exists expr {
            std::move(graph),
    };
    std::cout << expr << std::endl;
}

} // namespace yugawara::extension::scalar
