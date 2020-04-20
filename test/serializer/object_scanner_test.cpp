#include <yugawara/serializer/object_scanner.h>

#include <gtest/gtest.h>

#include <takatori/serializer/json_printer.h>

#include <takatori/value/primitive.h>

#include <takatori/type/primitive.h>

#include <takatori/scalar/immediate.h>

#include <takatori/plan/forward.h>

#include <yugawara/binding/factory.h>

#include <yugawara/type/extensions/error.h>
#include <yugawara/type/extensions/pending.h>

#include <yugawara/variable/comparison.h>
#include <yugawara/variable/negation.h>
#include <yugawara/variable/quantification.h>

#include <yugawara/analyzer/variable_mapping.h>
#include <yugawara/analyzer/expression_mapping.h>

namespace yugawara::serializer {

namespace t = ::takatori::type;
namespace v = ::takatori::value;
namespace scalar = ::takatori::scalar;

class object_scanner_test : public ::testing::Test {
public:
    void TearDown() override {
        std::cout << ::testing::UnitTest::GetInstance()->current_test_info()->name()
                  << ": " << buf_.str() << std::endl;
    }

    template<class T>
    void print(T const& element) {
        ::takatori::serializer::json_printer printer { buf_ };
        object_scanner scanner {
                variables_,
                expressions_
        };
        scanner(element, printer);
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
                            variable::nullable,
                            v::int4 { 100 },
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
    variable::declaration v {
            "X",
            t::int4 {},
    };

    print(bindings(v));
}

TEST_F(object_scanner_test, index) {
    auto t = std::make_shared<storage::table>(storage::table {
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
    }, ::takatori::util::object_creator {});
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
            aggregate::declaration::quantifier_type::all,
            t::int4 {},
            {
                    t::boolean {},
                    t::int8 {},
            },
            true,
    }));
}

TEST_F(object_scanner_test, type_error) {
    print(type::extensions::error {});
}

TEST_F(object_scanner_test, type_pending) {
    print(type::extensions::pending {});
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
    variable::declaration v {
            "X",
            t::int4 {},
            {
                    variable::nullable,
                    variable::comparison {
                            variable::comparison_operator::equal,
                            v::int4 { 0 },
                    },
            }
    };

    print(bindings(v));
}

TEST_F(object_scanner_test, predicate_negation) {
    variable::declaration v {
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
    };

    print(bindings(v));
}

TEST_F(object_scanner_test, predicate_quantification) {
    variable::declaration v {
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
    };

    print(bindings(v));
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
                            variable::nullable,
                            v::int4 { 100 },
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
            aggregate::declaration::quantifier_type::all,
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

} // namespace yugawara::serializer
