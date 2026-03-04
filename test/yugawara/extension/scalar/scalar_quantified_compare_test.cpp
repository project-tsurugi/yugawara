#include <yugawara/extension/scalar/quantified_compare.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <takatori/value/primitive.h>

#include <takatori/scalar/immediate.h>

#include <takatori/relation/graph.h>
#include <takatori/relation/values.h>

#include <takatori/util/clonable.h>

#include <yugawara/binding/factory.h>

namespace yugawara::extension::scalar {

class scalar_quantified_compare_test : public ::testing::Test {
public:
    binding::factory bindings;
};

static_assert(quantified_compare::tag == ::takatori::scalar::extension::tag);
static_assert(quantified_compare::extension_tag == quantified_compare_id);

using ::takatori::util::clone_unique;

static ::takatori::scalar::immediate constant(int v) {
    return ::takatori::scalar::immediate {
            ::takatori::value::int4 { v },
            ::takatori::type::int4  {},
    };
}

TEST_F(scalar_quantified_compare_test, simple) {
    auto i0 = bindings.stream_variable("i0");

    ::takatori::relation::graph_type graph;
    graph.insert(::takatori::relation::values {
            {
                    i0,
            },
            {
                    {
                            constant(2),
                    },
            },
    });
    quantified_compare expr {
            ::takatori::scalar::comparison_operator::equal,
            ::takatori::scalar::quantifier::any,
            constant(1),
            std::move(graph),
            i0,
    };

    EXPECT_EQ(expr.query_graph().size(), 1);

    auto output = expr.find_output_port();
    ASSERT_TRUE(output);
    EXPECT_TRUE(expr.query_graph().contains(output->owner()));

    EXPECT_EQ(expr.operator_kind(), ::takatori::scalar::comparison_operator::equal);
    EXPECT_EQ(expr.quantifier(), ::takatori::scalar::quantifier::any);
    EXPECT_EQ(expr.left(), constant(1));
    EXPECT_EQ(expr.right_column(), i0);
}

TEST_F(scalar_quantified_compare_test, clone) {
    auto i0 = bindings.stream_variable("i0");

    ::takatori::relation::graph_type graph;
    graph.insert(::takatori::relation::values {
            {
                    i0,
            },
            {
                    {
                            constant(2),
                    },
            },
    });
    quantified_compare expr {
            ::takatori::scalar::comparison_operator::equal,
            ::takatori::scalar::quantifier::any,
            constant(1),
            std::move(graph),
            i0,
    };

    auto copy = clone_unique(expr);
    EXPECT_NE(std::addressof(expr), copy.get());

    EXPECT_EQ(copy->query_graph().size(), 1);

    auto output = copy->find_output_port();
    ASSERT_TRUE(output);
    EXPECT_EQ(output->owner().kind(), ::takatori::relation::values::tag);
    EXPECT_TRUE(copy->query_graph().contains(output->owner()));
    EXPECT_FALSE(expr.query_graph().contains(output->owner()));

    EXPECT_EQ(copy->operator_kind(), expr.operator_kind());
    EXPECT_EQ(copy->quantifier(), expr.quantifier());
    EXPECT_EQ(copy->left().kind(), expr.left().kind());
    EXPECT_EQ(copy->right_column(), expr.right_column());
}

TEST_F(scalar_quantified_compare_test, output) {
    auto i0 = bindings.stream_variable("i0");

    ::takatori::relation::graph_type graph;
    graph.insert(::takatori::relation::values {
            {
                    i0,
            },
            {
                    {
                            constant(2),
                    },
            },
    });
    quantified_compare expr {
            ::takatori::scalar::comparison_operator::equal,
            ::takatori::scalar::quantifier::any,
            constant(1),
            std::move(graph),
            i0,
    };
    std::cout << expr << std::endl;
}

} // namespace yugawara::extension::scalar
