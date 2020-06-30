#include <yugawara/analyzer/expression_analyzer.h>

#include <gtest/gtest.h>

#include <takatori/document/basic_document.h>
#include <takatori/document/region.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/bit.h>
#include <takatori/type/time_point.h>
#include <takatori/type/datetime_interval.h>
#include <takatori/value/primitive.h>

#include <takatori/scalar/immediate.h>
#include <takatori/scalar/variable_reference.h>
#include <takatori/scalar/unary.h>
#include <takatori/scalar/binary.h>
#include <takatori/scalar/compare.h>
#include <takatori/scalar/match.h>
#include <takatori/scalar/conditional.h>
#include <takatori/scalar/coalesce.h>
#include <takatori/scalar/let.h>
#include <takatori/scalar/function_call.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/binding/factory.h>
#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>
#include <yugawara/extension/scalar/aggregate_function_call.h>

namespace yugawara::analyzer {

namespace s = ::takatori::scalar;
namespace t = ::takatori::type;
namespace v = ::takatori::value;

namespace ex = ::yugawara::extension::type;
namespace es = ::yugawara::extension::scalar;

using code = expression_analyzer_code;
using vref = ::takatori::scalar::variable_reference;

class expression_analyzer_scalar_test : public ::testing::Test {
public:
    bool ok() noexcept {
        return !analyzer.has_diagnostics();
    }

    template<class T>
    T&& bless(T&& t) noexcept {
        t.region() = { doc_, doc_cursor_ };
        return std::forward<T>(t);
    }

    ::takatori::util::optional_ptr<expression_analyzer::diagnostic_type const> find(
            ::takatori::scalar::expression const& expr,
            expression_analyzer_code code) {
        return find(expr.region(), code);
    }

    ::takatori::util::optional_ptr<expression_analyzer::diagnostic_type const> find(
            ::takatori::document::region region,
            expression_analyzer_code code) {
        if (!region) {
            throw std::invalid_argument("unknown region");
        }
        for (auto&& d : analyzer.diagnostics()) {
            if (code == d.code() && region == d.location()) {
                return d;
            }
        }
        return {};
    }

    ::takatori::descriptor::variable decl(::takatori::type::data&& type) {
        auto var = bindings.local_variable();
        analyzer.variables().bind(var, std::move(type));
        return var;
    }

protected:
    expression_analyzer analyzer;
    type::repository repo;
    binding::factory bindings;

    ::takatori::document::basic_document doc_ {
            "testing.sql",
            "01234567890123456789012345678901234567890123456789",
    };
    std::size_t doc_cursor_ {};
};

TEST_F(expression_analyzer_scalar_test, immediate) {
    s::immediate expr {
            v::int4 { 100 },
            t::int4 {},
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, variable_reference) {
    s::variable_reference expr {
            bindings.local_variable(),
    };
    analyzer.variables().bind(expr.variable(), t::int4());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, variable_reference_unbound) {
    s::variable_reference expr {
            bindings.local_variable(),
    };
    bless(expr);
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr, code::unresolved_variable));
}

TEST_F(expression_analyzer_scalar_test, unary_plus_numeric) {
    s::unary expr {
            s::unary_operator::plus,
            vref { decl(t::int4 {}) },
    };
    bless(expr);
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_plus_time_interval) {
    s::unary expr {
            s::unary_operator::plus,
            vref { decl(t::datetime_interval {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_plus_pending) {
    s::unary expr {
            s::unary_operator::plus,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::pending());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_plus_error) {
    s::unary expr {
            s::unary_operator::plus,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_plus_unknow) {
    s::unary expr {
            s::unary_operator::plus,
            vref { decl(t::unknown()) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.operand(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, unary_plus_invalid) {
    s::unary expr {
            s::unary_operator::plus,
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.operand(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, unary_sign_inversion_numeric) {
    s::unary expr {
            s::unary_operator::sign_inversion,
            vref { decl(t::int4 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_sign_inversion_time_interval) {
    s::unary expr {
            s::unary_operator::sign_inversion,
            vref { decl(t::datetime_interval {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_sign_inversion_pending) {
    s::unary expr {
            s::unary_operator::sign_inversion,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::pending());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_sign_inversion_error) {
    s::unary expr {
            s::unary_operator::sign_inversion,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_sign_inversion_unknow) {
    s::unary expr {
            s::unary_operator::sign_inversion,
            vref { decl(t::unknown()) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.operand(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, unary_sign_inversion_invalid) {
    s::unary expr {
            s::unary_operator::sign_inversion,
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.operand(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, unary_length_character) {
    s::unary expr {
            s::unary_operator::length,
            vref { decl(t::character { t::varying, 64 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_length_bit) {
    s::unary expr {
            s::unary_operator::length,
            vref { decl(t::bit { 64 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_length_pending) {
    s::unary expr {
            s::unary_operator::length,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_length_error) {
    s::unary expr {
            s::unary_operator::length,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_length_unknow) {
    s::unary expr {
            s::unary_operator::length,
            vref { decl(t::unknown()) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(find(expr.operand(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, unary_length_invalid) {
    s::unary expr {
            s::unary_operator::length,
            vref { decl(t::int4 {}) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(find(expr.operand(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, unary_conditional_not) {
    s::unary expr {
            s::unary_operator::conditional_not,
            vref { decl(t::boolean {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_conditional_not_pending) {
    s::unary expr {
            s::unary_operator::conditional_not,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_conditional_not_error) {
    s::unary expr {
            s::unary_operator::conditional_not,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_conditional_not_unknown) {
    s::unary expr {
            s::unary_operator::conditional_not,
            vref { decl(t::unknown()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_conditional_not_invalid) {
    s::unary expr {
            s::unary_operator::conditional_not,
            vref { decl(t::int4 {}) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.operand(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, unary_is_null) {
    s::unary expr {
            s::unary_operator::is_null,
            vref { decl(t::int4 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_null_pending) {
    s::unary expr {
            s::unary_operator::is_null,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_null_error) {
    s::unary expr {
            s::unary_operator::is_null,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_null_unknown) {
    s::unary expr {
            s::unary_operator::is_null,
            vref { decl(t::unknown()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_true) {
    s::unary expr {
            s::unary_operator::is_true,
            vref { decl(t::boolean {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_true_pending) {
    s::unary expr {
            s::unary_operator::is_true,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_true_error) {
    s::unary expr {
            s::unary_operator::is_true,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_true_unknown) {
    s::unary expr {
            s::unary_operator::is_true,
            vref { decl(t::unknown()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_true_invalid) {
    s::unary expr {
            s::unary_operator::is_true,
            vref { decl(t::int4 {}) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.operand(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, unary_is_false) {
    s::unary expr {
            s::unary_operator::is_false,
            vref { decl(t::boolean {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_false_pending) {
    s::unary expr {
            s::unary_operator::is_false,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_false_error) {
    s::unary expr {
            s::unary_operator::is_false,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_false_unknown) {
    s::unary expr {
            s::unary_operator::is_false,
            vref { decl(t::unknown()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_false_invalid) {
    s::unary expr {
            s::unary_operator::is_false,
            vref { decl(t::int4 {}) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.operand(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, unary_is_unknown) {
    s::unary expr {
            s::unary_operator::is_unknown,
            vref { decl(t::boolean {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_unknown_pending) {
    s::unary expr {
            s::unary_operator::is_unknown,
            vref { decl(ex::pending {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_unknown_error) {
    s::unary expr {
            s::unary_operator::is_unknown,
            vref { decl(ex::error {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_unknown_unknown) {
    s::unary expr {
            s::unary_operator::is_unknown,
            vref { decl(t::unknown()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, unary_is_unknown_invalid) {
    s::unary expr {
            s::unary_operator::is_unknown,
            vref { decl(t::int4 {}) },
    };
    bless(expr.operand());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.operand(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_add_number) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::int4 {}) },
            vref { decl(t::int8 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_add_decimal) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::decimal { 10, 2 }) },
            vref { decl(t::decimal { 20, 0 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::decimal(23, 2));
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_add_number_invalid) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::int4 {}) },
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_add_temporal) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::time_point {}) },
            vref { decl(t::datetime_interval {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::time_point());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_add_temporal_invalid) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::time_point {}) },
            vref { decl(t::int4 {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_add_time_interval) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::datetime_interval {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_add_time_interval_time_point) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::time_point {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::time_point());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_add_time_interval_invalid) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::int4 {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_add_unknown) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, binary_add_invalid) {
    s::binary expr {
            s::binary_operator::add,
            vref { decl(t::boolean()) },
            vref { decl(t::boolean()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_number) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::int4 {}) },
            vref { decl(t::int8 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_decimal) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::decimal { 10, 2 }) },
            vref { decl(t::decimal { 20, 0 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::decimal(23, 2));
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_number_invalid) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::int4 {}) },
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_temporal) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::time_point {}) },
            vref { decl(t::datetime_interval {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::time_point());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_temporal_invalid) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::time_point {}) },
            vref { decl(t::int4 {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_time_interval) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::datetime_interval {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_time_interval_time_point) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::time_point {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_time_interval_invalid) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::int4 {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_unknown) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, binary_subtract_invalid) {
    s::binary expr {
            s::binary_operator::subtract,
            vref { decl(t::boolean()) },
            vref { decl(t::boolean()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_number) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::int4 {}) },
            vref { decl(t::int8 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_decimal) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::decimal { 10, 2 }) },
            vref { decl(t::decimal { 20, 5 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::decimal(30, 7));
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_number_time_interval) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::int4 {}) },
            vref { decl(t::datetime_interval {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_number_invalid) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::int4 {}) },
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_time_interval) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::int4 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_time_interval_invalid) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::time_point {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_unknown) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, binary_multiply_invalid) {
    s::binary expr {
            s::binary_operator::multiply,
            vref { decl(t::boolean()) },
            vref { decl(t::boolean()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_divide_number) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::int4 {}) },
            vref { decl(t::int8 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_divide_decimal) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::decimal { 10, 2 }) },
            vref { decl(t::decimal { 20, 5 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::decimal(30, 7));
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_divide_number_time_interval) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::int4 {}) },
            vref { decl(t::datetime_interval {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_divide_number_invalid) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::int4 {}) },
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_divide_time_interval) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::int4 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_divide_time_interval_invalid) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::time_point {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_divide_unknown) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, binary_divide_invalid) {
    s::binary expr {
            s::binary_operator::divide,
            vref { decl(t::boolean()) },
            vref { decl(t::boolean()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_number) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::int4 {}) },
            vref { decl(t::int8 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_decimal) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::decimal { 10, 2 }) },
            vref { decl(t::decimal { 20, 5 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::decimal(30, 7));
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_number_time_interval) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::int4 {}) },
            vref { decl(t::datetime_interval {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_number_invalid) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::int4 {}) },
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_time_interval) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::int4 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::datetime_interval());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_time_interval_invalid) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::datetime_interval {}) },
            vref { decl(t::time_point {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_unknown) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, binary_remainder_invalid) {
    s::binary expr {
            s::binary_operator::remainder,
            vref { decl(t::boolean()) },
            vref { decl(t::boolean()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_concat_character) {
    s::binary expr {
            s::binary_operator::concat,
            vref { decl(t::character { t::varying, 10 }) },
            vref { decl(t::character { t::varying, 20 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::character(t::varying, 30));
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_concat_character_invalid) {
    s::binary expr {
            s::binary_operator::concat,
            vref { decl(t::character { t::varying, 10 }) },
            vref { decl(t::boolean()) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_concat_bit) {
    s::binary expr {
            s::binary_operator::concat,
            vref { decl(t::bit { t::varying, 10 }) },
            vref { decl(t::bit { t::varying, 20 }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::bit(t::varying, 30));
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_concat_bit_invalid) {
    s::binary expr {
            s::binary_operator::concat,
            vref { decl(t::bit { t::varying, 10 }) },
            vref { decl(t::boolean()) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_concat_unknown) {
    s::binary expr {
            s::binary_operator::concat,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::ambiguous_type));
}

TEST_F(expression_analyzer_scalar_test, binary_concat_invalid) {
    s::binary expr {
            s::binary_operator::concat,
            vref { decl(t::boolean()) },
            vref { decl(t::boolean()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_and) {
    s::binary expr {
            s::binary_operator::conditional_and,
            vref { decl(t::boolean {}) },
            vref { decl(t::boolean {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_and_boolean_invalid) {
    s::binary expr {
            s::binary_operator::conditional_and,
            vref { decl(t::boolean {}) },
            vref { decl(t::int4 {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_and_unknown) {
    s::binary expr {
            s::binary_operator::conditional_and,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_and_invalid) {
    s::binary expr {
            s::binary_operator::conditional_and,
            vref { decl(t::int4()) },
            vref { decl(t::int4()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_or) {
    s::binary expr {
            s::binary_operator::conditional_or,
            vref { decl(t::boolean {}) },
            vref { decl(t::boolean {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_or_boolean_invalid) {
    s::binary expr {
            s::binary_operator::conditional_or,
            vref { decl(t::boolean {}) },
            vref { decl(t::int4 {}) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_or_unknown) {
    s::binary expr {
            s::binary_operator::conditional_or,
            vref { decl(t::unknown()) },
            vref { decl(t::unknown()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, binary_conditional_or_invalid) {
    s::binary expr {
            s::binary_operator::conditional_or,
            vref { decl(t::int4()) },
            vref { decl(t::int4()) },
    };
    bless(expr.left());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.left(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, compare) {
    s::compare expr {
            s::comparison_operator::equal,
            vref { decl(t::int4()) },
            vref { decl(t::int8()) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, compare_invalid) {
    s::compare expr {
            s::comparison_operator::equal,
            vref { decl(t::int4()) },
            vref { decl(t::boolean()) },
    };
    bless(expr.right());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.right(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, match) {
    s::match expr {
            s::match_operator::like,
            vref { decl(t::character { t::varying }) },
            vref { decl(t::character { t::varying }) },
            vref { decl(t::character { t::varying }) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, match_input_invalid) {
    s::match expr {
            s::match_operator::like,
            vref { decl(t::boolean {}) },
            vref { decl(t::character { t::varying }) },
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.input());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.input(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, match_pattern_invalid) {
    s::match expr {
            s::match_operator::like,
            vref { decl(t::character { t::varying }) },
            vref { decl(t::boolean {}) },
            vref { decl(t::character { t::varying }) },
    };
    bless(expr.pattern());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.pattern(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, match_escape_invalid) {
    s::match expr {
            s::match_operator::like,
            vref { decl(t::character { t::varying }) },
            vref { decl(t::character { t::varying }) },
            vref { decl(t::boolean {}) },
    };
    bless(expr.escape());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::boolean());
    EXPECT_TRUE(find(expr.escape(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, conditional) {
    s::conditional expr {
            {
                    s::conditional::alternative {
                            vref { decl(t::boolean {}) },
                            vref { decl(t::int4 {}) },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, conditional_unify) {
    s::conditional expr {
            {
                    s::conditional::alternative {
                            vref { decl(t::boolean {}) },
                            vref { decl(t::int4 {}) },
                    },
                    s::conditional::alternative {
                            vref { decl(t::boolean {}) },
                            vref { decl(t::int8 {}) },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, conditional_else) {
    s::conditional expr {
            {
                    s::conditional::alternative {
                            vref { decl(t::boolean {}) },
                            vref { decl(t::int4 {}) },
                    },
            },
            vref { decl(t::int8 {}) },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, conditional_condition_invalid) {
    s::conditional expr {
            {
                    s::conditional::alternative {
                            vref { decl(t::character { t::varying }) },
                            vref { decl(t::int4 {}) },
                    },
            },
    };
    bless(expr.alternatives()[0].condition());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(find(expr.alternatives()[0].condition(), code::unsupported_type));
}

TEST_F(expression_analyzer_scalar_test, conditional_body_invalid) {
    s::conditional expr {
            {
                    s::conditional::alternative {
                            vref { decl(t::boolean {}) },
                            vref { decl(t::int4 {}) },
                    },
            },
            vref { decl(t::boolean {}) },
    };
    bless(*expr.default_expression());
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(*expr.default_expression(), code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, coalesce) {
    s::coalesce expr {
            {
                    vref { decl(t::int4 {}) },
            }
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, coalesce_multiple) {
    s::coalesce expr {
            {
                    vref { decl(t::int4 {}) },
                    vref { decl(t::unknown {}) },
                    vref { decl(t::int8 {}) },
            }
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, coalesce_unresolved) {
    s::coalesce expr {
            {
                    vref { decl(t::int4 {}) },
                    vref { decl(ex::pending {}) },
                    vref { decl(t::int8 {}) },
            }
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::pending());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, coalesce_invalid) {
    s::coalesce expr {
            {
                    vref { decl(t::int4 {}) },
                    vref { decl(t::boolean {}) },
                    vref { decl(t::int8 {}) },
            }
    };
    bless(expr.alternatives()[1]);
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, ex::error());
    EXPECT_TRUE(find(expr.alternatives()[1], code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, let) {
    auto v = bindings.local_variable();
    s::let expr {
            s::let::declarator {
                    v,
                    vref { decl(t::int4 {}) },
            },
            vref { v },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, let_multiple) {
    auto v1 = bindings.local_variable();
    auto v2 = bindings.local_variable();
    auto v3 = bindings.local_variable();
    s::let expr {
            {
                    s::let::declarator {
                            v1,
                            vref { decl(t::int4 {}) },
                    },
                    s::let::declarator {
                            v2,
                            vref { decl(t::int8 {}) },
                    },
                    s::let::declarator {
                            v3,
                            vref { decl(t::unknown {}) },
                    },
            },
            s::coalesce {
                    {
                            vref { v1 },
                            vref { v2 },
                            vref { v3 },
                    },
            },
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int8());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, function_call) {
    auto f = bindings.function({
            function::declaration::minimum_user_function_id + 100,
            "testing",
            t::int4 {},
            {},
    });
    s::function_call expr {
            f,
            {},
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, function_call_inconsistent) {
    auto f = bindings.function({
            function::declaration::minimum_user_function_id + 100,
            "testing",
            t::int4 {},
            {
                    t::character(t::varying),
            },
    });
    s::function_call expr {
            f,
            {
                    vref { decl(t::int4 {}) },
            },
    };
    bless(expr.arguments()[0]);
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(find(expr.arguments()[0], code::inconsistent_type));
}

TEST_F(expression_analyzer_scalar_test, function_call_unresolved) {
    auto f = bindings.function({
            function::declaration::unresolved_definition_id,
            "testing",
            t::int4 {},
            {},
    });
    s::function_call expr {
            f,
            {},
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, aggregate_function_call) {
    auto f = bindings.aggregate_function({
            aggregate::declaration::minimum_user_function_id + 100,
            "testing",
            t::int4 {},
            {},
    });
    es::aggregate_function_call expr {
            f,
            {},
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, aggregate_function_call_unresolved) {
    auto f = bindings.aggregate_function({
            aggregate::declaration::unresolved_definition_id,
            "testing",
            t::int4 {},
            {},
    });
    es::aggregate_function_call expr {
            f,
            {},
    };
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(ok());
}

TEST_F(expression_analyzer_scalar_test, aggregate_function_call_inconsistent) {
    auto f = bindings.aggregate_function({
            aggregate::declaration::unresolved_definition_id,
            "testing",
            t::int4 {},
            {
                    t::character(t::varying),
            },
    });
    es::aggregate_function_call expr {
            f,
            {
                    vref { decl(t::int4 {}) },
            },
    };
    bless(expr.arguments()[0]);
    auto r = analyzer.resolve(expr, true, repo);
    EXPECT_EQ(*r, t::int4());
    EXPECT_TRUE(find(expr.arguments()[0], code::inconsistent_type));
}

} // namespace yugawara::analyzer
