#include <yugawara/serializer/object_scanner.h>

#include <gtest/gtest.h>

#include <takatori/serializer/json_printer.h>

#include <takatori/value/primitive.h>

#include <takatori/type/primitive.h>

#include <takatori/scalar/immediate.h>

#include <takatori/plan/forward.h>

#include <yugawara/binding/factory.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

#include <yugawara/variable/comparison.h>
#include <yugawara/variable/negation.h>
#include <yugawara/variable/quantification.h>
#include <yugawara/extension/scalar/aggregate_function_call.h>

namespace yugawara::serializer {

namespace t = ::takatori::type;
namespace v = ::takatori::value;
namespace scalar = ::takatori::scalar;

class object_scanner_test : public ::testing::Test {
public:
    template<class T>
    void print(T const& element) {
        std::cout << ::testing::UnitTest::GetInstance()->current_test_info()->name() << ": ";

        ::takatori::serializer::json_printer printer { std::cout };
        ::yugawara::serializer::object_scanner scanner { variables_, expressions_ };
        scanner(element, printer);

        std::cout << std::endl;
        if (printer.depth() != 0) {
            throw std::domain_error("invalid JSON depth");
        }
    }

    analyzer::variable_mapping& variables() noexcept {
        return *variables_;
    }

    analyzer::expression_mapping& expressions() noexcept {
        return *expressions_;
    }

    binding::factory bindings; // NOLINT

private:
    std::ostringstream buf_ {};
    std::shared_ptr<analyzer::variable_mapping> variables_ = std::make_shared<analyzer::variable_mapping>();
    std::shared_ptr<analyzer::expression_mapping> expressions_ = std::make_shared<analyzer::expression_mapping>();
};

TEST_F(object_scanner_test, table_column) {
    storage::table t {
            "T",
            {
                    {
                            "C0",
                            t::int4 {},
                    },
            },
    };
    print(bindings(t.columns()[0]));
}

TEST_F(object_scanner_test, table_column_default_immediate) {
    storage::table t {
            "T",
            {
                    {
                            "C0",
                            t::int4 {},
                            variable::nullable,
                            { v::int4 { 100 } },
                    },
            },
    };
    print(bindings(t.columns()[0]));
}

TEST_F(object_scanner_test, table_column_default_sequence) {
    auto seq = std::make_shared<storage::sequence>("S1");
    storage::table t {
            "T",
            {
                    {
                            "C0",
                            t::int4 {},
                            variable::nullable,
                            { seq },
                    },
            },
    };
    print(bindings(t.columns()[0]));
}

TEST_F(object_scanner_test, exchange_column) {
    print(bindings.exchange_column("x"));
}

TEST_F(object_scanner_test, frame_variable) {
    print(bindings.frame_variable("f"));
}

TEST_F(object_scanner_test, stream_variable) {
    print(bindings.stream_variable("s"));
}

TEST_F(object_scanner_test, local_variable) {
    print(bindings.local_variable("l"));
}

TEST_F(object_scanner_test, external_variable) {
    print(bindings( variable::declaration {
            "X",
            t::int4 {},
    }));
}

TEST_F(object_scanner_test, index) {
    auto t = std::make_shared<storage::table>(::takatori::util::clone_tag, storage::table {
            "T",
            {
                    {
                            "C0",
                            t::int4 {},
                    },
                    {
                            "C1",
                            t::int4 {},
                    },
                    {
                            "C2",
                            t::int4 {},
                    },
            },
    });
    print(bindings(storage::index {
            t,
            "IDX",
            {
                    { t->columns()[0], storage::sort_direction::ascendant },
                    { t->columns()[1], storage::sort_direction::descendant },
            },
            {
                    t->columns()[2],
            },
    }));
}

TEST_F(object_scanner_test, exchange) {
    print(bindings(::takatori::plan::forward {}));
}

TEST_F(object_scanner_test, function) {
    print(bindings(function::declaration{
            1000,
            "f",
            t::int4{},
            {
                    t::boolean{},
                    t::int8{},
            },
    }));
}

TEST_F(object_scanner_test, aggregate_function) {
    print(bindings(aggregate::declaration {
            1000,
            "f",
            t::int4 {},
            {
                    t::boolean {},
                    t::int8 {},
            },
            true,
    }));
}

TEST_F(object_scanner_test, type_error) {
    print(extension::type::error {});
}

TEST_F(object_scanner_test, type_pending) {
    print(extension::type::pending {});
}

TEST_F(object_scanner_test, scalar_expression) {
    scalar::immediate expr {
            v::int4 { 100 },
            t::int4 {}
    };
    print(expr);
}

TEST_F(object_scanner_test, scalar_expression_resolution) {
    scalar::immediate expr {
            v::int4 { 100 },
            t::int4 {}
    };
    expressions().bind(expr, t::int4 {});
    print(expr);
}

TEST_F(object_scanner_test, predicate_comparison) {
    print(bindings(variable::declaration {
            "X",
            t::int4 {},
            {
                    variable::nullable,
                    variable::comparison {
                            variable::comparison_operator::equal,
                            v::int4 { 0 },
                    },
            }
    }));
}

TEST_F(object_scanner_test, predicate_negation) {
    print(bindings(variable::declaration {
            "X",
            t::int4 {},
            {
                    variable::nullable,
                    variable::negation {
                            variable::comparison {
                                    variable::comparison_operator::equal,
                                    v::int4 { 0 },
                            },
                    },
            }
    }));
}

TEST_F(object_scanner_test, predicate_quantification) {
    print(bindings(variable::declaration {
            "X",
            t::int4 {},
            {
                    variable::nullable,
                    variable::quantification {
                            variable::quantifier::all,
                            {
                                    variable::comparison {
                                            variable::comparison_operator::greater_equal,
                                            v::int4 { 0 },
                                    },
                                    variable::comparison {
                                            variable::comparison_operator::less_equal,
                                            v::int4 { 100 },
                                    },
                            }
                    },
            }
    }));
}

TEST_F(object_scanner_test, resolution_unresolved) {
    auto v = bindings.stream_variable("c");
    variables().bind(v, {});
    print(v);
}

TEST_F(object_scanner_test, resolution_unknown) {
    auto v = bindings.stream_variable("c");
    variables().bind(v, t::int4 {});
    print(v);
}

TEST_F(object_scanner_test, resolution_scalar_expression) {
    scalar::immediate expr {
            v::int4 { 100 },
            t::int4 {}
    };
    auto v = bindings.stream_variable("c");
    variables().bind(v, expr);
    print(v);
}

TEST_F(object_scanner_test, resolution_table_column) {
    storage::table t {
            "T",
            {
                    {
                            "C0",
                            t::int4 {},
                    },
            },
    };
    auto v = bindings.stream_variable("c");
    variables().bind(v, t.columns()[0]);
    print(v);
}

TEST_F(object_scanner_test, resolution_external) {
    variable::declaration decl {
            "X",
            t::int4 {},
    };
    auto v = bindings.stream_variable("c");
    variables().bind(v, decl);
    print(v);
}

TEST_F(object_scanner_test, resolution_function_call) {
    function::declaration decl {
            1000,
            "f",
            t::int4 {},
            {
                    t::boolean {},
                    t::int8 {},
            },
    };

    auto v = bindings.stream_variable("c");
    variables().bind(v, decl);
    print(v);
}

TEST_F(object_scanner_test, resolution_aggregation) {
    aggregate::declaration decl {
            1000,
            "f",
            t::int4 {},
            {
                    t::boolean {},
                    t::int8 {},
            },
            true,
    };

    auto v = bindings.stream_variable("c");
    variables().bind(v, decl);
    print(v);
}

TEST_F(object_scanner_test, extension_scalar_aggregate_function_call) {
    print(extension::scalar::aggregate_function_call {
            bindings(aggregate::declaration {
                1000,
                        "f",
                        t::int4 {},
                        {
                                t::boolean {},
                                t::int8 {},
                        },
                        true,
            }),
            {
                    scalar::immediate {
                            v::boolean { true },
                            t::boolean {}
                    },
                    scalar::immediate {
                            v::int8 { 100 },
                            t::int8 {}
                    },
            }
    });
}

} // namespace yugawara::serializer
