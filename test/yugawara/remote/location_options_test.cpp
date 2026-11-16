#include <yugawara/remote/location_options.h>

#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include <yugawara/testing/mock_symbol_location_query.h>

namespace yugawara::remote {

using ::yugawara::testing::mock_symbol_location_query;

class location_options_test : public ::testing::Test {};

namespace {

template<class Enum>
constexpr std::size_t count_of(Enum first, Enum last) noexcept {
    using underlying = std::underlying_type_t<Enum>;
    return static_cast<std::size_t>(static_cast<underlying>(last) - static_cast<underlying>(first)) + 1;
}

} // namespace

TEST_F(location_options_test, defaults) {
    location_options options {};
    EXPECT_FALSE(options.symbol_location_query());
    EXPECT_FALSE(options.allow_empty_operation_location());

    namespace relation = ::takatori::relation;
    namespace scalar = ::takatori::scalar;
    namespace plan = ::takatori::plan;
    namespace statement = ::takatori::statement;

    auto&& rs = options.remote_relation_expression_kinds();
    EXPECT_EQ(rs.size(), count_of(relation::expression_kind::find, relation::expression_kind::extension));
    EXPECT_TRUE(rs.contains(relation::expression_kind::find));
    EXPECT_TRUE(rs.contains(relation::expression_kind::write));
    EXPECT_TRUE(rs.contains(relation::expression_kind::take_flat));
    EXPECT_TRUE(rs.contains(relation::expression_kind::offer));
    EXPECT_TRUE(rs.contains(relation::expression_kind::extension));

    auto&& ss = options.remote_scalar_expression_kinds();
    EXPECT_EQ(ss.size(), count_of(scalar::expression_kind::immediate, scalar::expression_kind::extension));
    EXPECT_TRUE(ss.contains(scalar::expression_kind::immediate));
    EXPECT_TRUE(ss.contains(scalar::expression_kind::function_call));
    EXPECT_TRUE(ss.contains(scalar::expression_kind::extension));

    auto&& ps = options.remote_step_kinds();
    EXPECT_EQ(ps.size(), count_of(plan::step_kind::process, plan::step_kind::discard));
    EXPECT_TRUE(ps.contains(plan::step_kind::process));
    EXPECT_TRUE(ps.contains(plan::step_kind::forward));
    EXPECT_TRUE(ps.contains(plan::step_kind::discard));

    auto&& ts = options.remote_statement_kinds();
    EXPECT_EQ(ts.size(), count_of(statement::statement_kind::execute, statement::statement_kind::extension));
    EXPECT_TRUE(ts.contains(statement::statement_kind::execute));
    EXPECT_TRUE(ts.contains(statement::statement_kind::write));
    EXPECT_TRUE(ts.contains(statement::statement_kind::extension));
}

TEST_F(location_options_test, modify_kinds) {
    namespace relation = ::takatori::relation;
    location_options options {};
    options.remote_relation_expression_kinds().erase(relation::expression_kind::write);
    EXPECT_FALSE(options.remote_relation_expression_kinds().contains(relation::expression_kind::write));
    EXPECT_TRUE(options.remote_relation_expression_kinds().contains(relation::expression_kind::scan));

    location_options fresh {};
    EXPECT_TRUE(fresh.remote_relation_expression_kinds().contains(relation::expression_kind::write));
}

TEST_F(location_options_test, copy) {
    auto query = std::make_shared<mock_symbol_location_query>();
    location_options options {};
    options.symbol_location_query() = query;
    options.remote_step_kinds().clear();

    location_options copy { options };
    EXPECT_EQ(copy.symbol_location_query().get(), query.get());
    EXPECT_TRUE(copy.remote_step_kinds().empty());

    copy.remote_step_kinds().insert(::takatori::plan::step_kind::process);
    EXPECT_TRUE(options.remote_step_kinds().empty());
}

} // namespace yugawara::remote
