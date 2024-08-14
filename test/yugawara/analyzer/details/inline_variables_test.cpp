#include <yugawara/analyzer/details/inline_variables.h>

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

class inline_variables_test : public ::testing::Test {
protected:

    scalar::variable_reference var(descriptor::variable v) {
        return scalar::variable_reference {
                std::move(v),
        };
    }

    scalar::immediate dummy(int v = 0) {
        return scalar::immediate {
                v::int4 { v },
                t::int4 {},
        };
    }

    descriptor::variable declare(int v) {
        auto r = bindings.stream_variable();
        inliner.declare(r, std::make_unique<scalar::immediate>(v::int4 { v }, t::int4 {}));
        return r;
    }

    std::unique_ptr<scalar::expression> apply(scalar::expression&& expr) {
        auto target = ::takatori::util::clone_unique(std::move(expr));
        inliner.apply(target);
        return target;
    }

    inline_variables inliner;
    binding::factory bindings;
};

TEST_F(inline_variables_test, immediate) {
    auto r = apply(scalar::immediate {
            v::int4 { 100 },
            t::int4 {},
    });
    EXPECT_EQ(*r, (scalar::immediate {
            v::int4 { 100 },
            t::int4 {},
    }));
}

TEST_F(inline_variables_test, variable_reference_miss) {
    auto v = bindings.stream_variable();
    auto r = apply(scalar::variable_reference {
            v,
    });
    EXPECT_EQ(*r, (scalar::variable_reference {
            v,
    }));
}

TEST_F(inline_variables_test, variable_reference_hit) {
    auto v = declare(10);
    auto r = apply(scalar::variable_reference {
            v,
    });
    EXPECT_EQ(*r, dummy(10));
}

TEST_F(inline_variables_test, unary_miss) {
    auto v = bindings.stream_variable();
    auto r = apply(scalar::unary {
            scalar::unary_operator::plus,
            var(v),
    });
    EXPECT_EQ(*r, (scalar::unary {
            scalar::unary_operator::plus,
            var(v),
    }));
}

TEST_F(inline_variables_test, unary_operand) {
    auto v = declare(10);
    auto r = apply(scalar::unary {
            scalar::unary_operator::plus,
            var(v),
    });
    EXPECT_EQ(*r, (scalar::unary {
            scalar::unary_operator::plus,
            dummy(10),
    }));
}

TEST_F(inline_variables_test, cast_miss) {
    auto v = bindings.stream_variable();
    auto r = apply(scalar::cast {
            t::int8 {},
            scalar::cast_loss_policy::error,
            var(v),
    });
    EXPECT_EQ(*r, (scalar::cast {
            t::int8 {},
            scalar::cast_loss_policy::error,
            var(v),
    }));
}

TEST_F(inline_variables_test, cast_operand) {
    auto v = declare(10);
    auto r = apply(scalar::cast {
            t::int8 {},
            scalar::cast_loss_policy::error,
            var(v),
    });
    EXPECT_EQ(*r, (scalar::cast {
            t::int8 {},
            scalar::cast_loss_policy::error,
            dummy(10),
    }));
}

TEST_F(inline_variables_test, binary_miss) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto r = apply(scalar::binary {
            scalar::binary_operator::add,
            var(v1),
            var(v2),
    });
    EXPECT_EQ(*r, (scalar::binary {
            scalar::binary_operator::add,
            var(v1),
            var(v2),
    }));
}

TEST_F(inline_variables_test, binary_hit) {
    auto v1 = declare(10);
    auto v2 = declare(20);
    auto r = apply(scalar::binary {
            scalar::binary_operator::add,
            var(v1),
            var(v2),
    });
    EXPECT_EQ(*r, (scalar::binary {
            scalar::binary_operator::add,
            dummy(10),
            dummy(20),
    }));
}

TEST_F(inline_variables_test, compare_miss) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto r = apply(scalar::compare {
            scalar::comparison_operator::equal,
            var(v1),
            var(v2),
    });
    EXPECT_EQ(*r, (scalar::compare {
            scalar::comparison_operator::equal,
            var(v1),
            var(v2),
    }));
}

TEST_F(inline_variables_test, compare_left) {
    auto v1 = declare(10);
    auto v2 = declare(20);
    auto r = apply(scalar::compare {
            scalar::comparison_operator::equal,
            var(v1),
            var(v2),
    });
    EXPECT_EQ(*r, (scalar::compare {
            scalar::comparison_operator::equal,
            dummy(10),
            dummy(20),
    }));
}

TEST_F(inline_variables_test, match_miss) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto v3 = bindings.stream_variable();
    auto r = apply(scalar::match {
            scalar::match_operator::like,
            var(v1),
            var(v2),
            var(v3),
    });
    EXPECT_EQ(*r, (scalar::match {
            scalar::match_operator::like,
            var(v1),
            var(v2),
            var(v3),
    }));
}

TEST_F(inline_variables_test, match_hit) {
    auto v1 = declare(10);
    auto v2 = declare(20);
    auto v3 = declare(30);
    auto r = apply(scalar::match {
            scalar::match_operator::like,
            var(v1),
            var(v2),
            var(v3),
    });
    EXPECT_EQ(*r, (scalar::match {
            scalar::match_operator::like,
            dummy(10),
            dummy(20),
            dummy(30),
    }));
}

TEST_F(inline_variables_test, conditional_miss) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto v3 = bindings.stream_variable();
    auto v4 = bindings.stream_variable();
    auto v5 = bindings.stream_variable();
    auto r = apply(scalar::conditional {
            {
                    scalar::conditional::alternative { var(v1), var(v2), },
                    scalar::conditional::alternative { var(v3), var(v4), },
            },
            var(v5),
    });
    EXPECT_EQ(*r, (scalar::conditional {
            {
                    scalar::conditional::alternative { var(v1), var(v2), },
                    scalar::conditional::alternative { var(v3), var(v4), },
            },
            var(v5),
    }));
}

TEST_F(inline_variables_test, conditional_hit) {
    auto v1 = declare(10);
    auto v2 = bindings.stream_variable();
    auto v3 = bindings.stream_variable();
    auto v4 = declare(20);
    auto v5 = declare(30);
    auto r = apply(scalar::conditional {
            {
                    scalar::conditional::alternative { var(v1), var(v2), },
                    scalar::conditional::alternative { var(v3), var(v4), },
            },
            var(v5),
    });
    EXPECT_EQ(*r, (scalar::conditional {
            {
                    scalar::conditional::alternative {dummy(10), var(v2), },
                    scalar::conditional::alternative { var(v3), dummy(20), },
            },
            dummy(30),
    }));
}

TEST_F(inline_variables_test, coalesce_miss) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto v3 = bindings.stream_variable();
    auto r = apply(scalar::coalesce {
            {
                    var(v1),
                    var(v2),
                    var(v3),
            }
    });
    EXPECT_EQ(*r, (scalar::coalesce {
            {
                    var(v1),
                    var(v2),
                    var(v3),
            }
    }));
}

TEST_F(inline_variables_test, coalesce_hit) {
    auto v1 = declare(10);
    auto v2 = declare(20);
    auto v3 = declare(30);
    auto r = apply(scalar::coalesce {
            {
                    var(v1),
                    var(v2),
                    var(v3),
            }
    });
    EXPECT_EQ(*r, (scalar::coalesce {
            {
                    dummy(10),
                    dummy(20),
                    dummy(30),
            }
    }));
}

TEST_F(inline_variables_test, let_miss) {
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto v3 = bindings.stream_variable();
    auto v4 = bindings.stream_variable();
    auto v5 = bindings.stream_variable();
    auto r = apply(scalar::let {
            {
                    scalar::let::variable { v1, var(v2) },
                    scalar::let::variable { v3, var(v4) },
            },
            var(v5),
    });
    EXPECT_EQ(*r, (scalar::let {
            {
                    scalar::let::variable { v1, var(v2) },
                    scalar::let::variable { v3, var(v4) },
            },
            var(v5),
    }));
}

TEST_F(inline_variables_test, let_hit) {
    auto v1 = bindings.stream_variable();
    auto v2 = declare(10);
    auto v3 = bindings.stream_variable();
    auto v4 = bindings.stream_variable();
    auto v5 = declare(20);
    auto r = apply(scalar::let {
            {
                    scalar::let::variable { v1, var(v2) },
                    scalar::let::variable { v3, var(v4) },
            },
            var(v5),
    });
    EXPECT_EQ(*r, (scalar::let {
            {
                    scalar::let::variable { v1, dummy(10) },
                    scalar::let::variable { v3, var(v4) },
            },
            dummy(20),
    }));
}

TEST_F(inline_variables_test, function_call_miss) {
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
    auto v1 = bindings.stream_variable();
    auto v2 = bindings.stream_variable();
    auto v3 = bindings.stream_variable();
    auto r = apply(scalar::function_call {
            f,
            {
                    var(v1),
                    var(v2),
                    var(v3),
            },
    });
    EXPECT_EQ(*r, (scalar::function_call {
            f,
            {
                    var(v1),
                    var(v2),
                    var(v3),
            },
    }));
}

TEST_F(inline_variables_test, function_call_hit) {
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
    auto v1 = declare(10);
    auto v2 = declare(20);
    auto v3 = declare(30);
    auto r = apply(scalar::function_call {
            f,
            {
                    var(v1),
                    var(v2),
                    var(v3),
            },
    });
    EXPECT_EQ(*r, (scalar::function_call {
            f,
            {
                    dummy(10),
                    dummy(20),
                    dummy(30),
            },
    }));
}

} // namespace yugawara::analyzer::details
