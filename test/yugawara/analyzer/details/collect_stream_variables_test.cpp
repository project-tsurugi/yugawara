#include <yugawara/analyzer/details/collect_stream_variables.h>

#include <gtest/gtest.h>

#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>
#include <takatori/scalar/unary.h>

#include <yugawara/binding/factory.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using details::collect_stream_variables;

class collect_stream_variables_test : public ::testing::Test {
protected:
    binding::factory bindings;

    std::vector<descriptor::variable> results;
    details::variable_consumer consumer = [this](descriptor::variable const& variable) {
        results.emplace_back(variable);
    };

    bool eq(std::initializer_list<descriptor::variable> as) {
        if (as.size() != results.size()) {
            return false;
        }
        // NOTE: no duplicates in as
        for (auto&& a : as) {
            bool found = false;
            for (auto it = results.begin(); it != results.end();) {
                if (a == *it) {
                    found = true;
                    it = results.erase(it);
                    break;
                } else {
                    ++it;
                }
            }
            if (!found) {
                return false;
            }
        }
        return true;
    }
};

TEST_F(collect_stream_variables_test, simple) {
    collect_stream_variables(constant(100), consumer);

    EXPECT_TRUE(eq({}));
}

TEST_F(collect_stream_variables_test, variable_reference) {
    auto c0 = bindings.stream_variable("c0");
    scalar::variable_reference expr { c0 };
    collect_stream_variables(expr, consumer);

    EXPECT_TRUE(eq({ c0 }));
}

TEST_F(collect_stream_variables_test, variable_reference_local) {
    auto c0 = bindings.local_variable("c0");
    scalar::variable_reference expr { c0 };
    collect_stream_variables(expr, consumer);

    EXPECT_TRUE(eq({}));
}

TEST_F(collect_stream_variables_test, nested) {
    auto c0 = bindings.stream_variable("c0");
    scalar::unary expr {
            scalar::unary_operator::conditional_not,
            scalar::variable_reference { c0 },
    };
    collect_stream_variables(expr, consumer);

    EXPECT_TRUE(eq({ c0 }));
}

TEST_F(collect_stream_variables_test, multiple) {
    auto c0 = bindings.stream_variable("c0");
    auto c1 = bindings.stream_variable("c0");
    auto c2 = bindings.stream_variable("c0");
    scalar::compare expr {
            scalar::comparison_operator::equal,
            scalar::variable_reference { c0 },
            scalar::binary {
                    scalar::binary_operator::add,
                    scalar::variable_reference { c1 },
                    scalar::variable_reference { c2 },
            },
    };
    collect_stream_variables(expr, consumer);

    EXPECT_TRUE(eq({ c0, c1, c2 }));
}

} // namespace yugawara::analyzer::details
