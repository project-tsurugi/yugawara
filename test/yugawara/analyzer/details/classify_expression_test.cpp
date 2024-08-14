#include <yugawara/analyzer/details/classify_expression.h>

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

#include <yugawara/binding/factory.h>
#include <yugawara/type/repository.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

class classify_expression_test : public ::testing::Test {
protected:
    void validate(::takatori::scalar::expression const& expr, expression_class_set expected) {
        auto r = classify_expression(expr);
        EXPECT_EQ(expected, r) << r;
    }
    
    scalar::immediate dummy() {
        return scalar::immediate {
                v::int4 { 1 },
                t::int4 {},
        };
    }

    type::repository types;
    binding::factory bindings;
};

TEST_F(classify_expression_test, immediate) {
    scalar::immediate expr {
            v::int4 {},
            t::int4 {},
    };
    validate(expr, {
            expression_class::constant,
            expression_class::trivial,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, variable_reference) {
    scalar::variable_reference expr {
            bindings.stream_variable(),
    };
    validate(expr, {
            expression_class::trivial,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, unary) {
    scalar::unary expr {
            scalar::unary_operator::plus,
            dummy()
    };
    validate(expr, {
            expression_class::constant,
            expression_class::trivial,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, cast) {
    scalar::cast expr {
            t::int8 {},
            scalar::cast_loss_policy::error,
            dummy()
    };
    validate(expr, {
            expression_class::constant,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, binary) {
    scalar::binary expr {
            scalar::binary_operator::add,
            dummy(),
            dummy(),
    };
    validate(expr, {
            expression_class::constant,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, compare) {
    scalar::compare expr {
            scalar::comparison_operator::equal,
            dummy(),
            dummy(),
    };
    validate(expr, {
            expression_class::constant,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, match) {
    scalar::match expr {
            scalar::match_operator::like,
            dummy(),
            dummy(),
            dummy(),
    };
    validate(expr, {});
}

TEST_F(classify_expression_test, conditional) {
    scalar::conditional expr {
            {
                    scalar::conditional::alternative { dummy(), dummy(), },
                    scalar::conditional::alternative { dummy(), dummy(), },
                    scalar::conditional::alternative { dummy(), dummy(), },
            },
            dummy(),
    };
    validate(expr, {
            expression_class::constant,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, coalesce) {
    scalar::coalesce expr {
            {
                    dummy(),
                    dummy(),
                    dummy(),
            }
    };
    validate(expr, {
            expression_class::constant,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, let) {
    scalar::let expr {
            {
                    scalar::let::variable { bindings.stream_variable(), dummy() },
                    scalar::let::variable { bindings.stream_variable(), dummy() },
                    scalar::let::variable { bindings.stream_variable(), dummy() },
            },
            dummy(),
    };
    validate(expr, {
            expression_class::constant,
            expression_class::small,
    });
}

TEST_F(classify_expression_test, function_call) {
    auto f = bindings.function({
            1,
            "f",
            t::int4 {},
            {
                    t::int4 {},
            },
    });
    scalar::function_call expr {
            f,
            {
                    dummy(),
            },
    };
    validate(expr, {});
}

} // namespace yugawara::analyzer::details
