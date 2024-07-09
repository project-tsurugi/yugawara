#include <yugawara/details/collect_restricted_features.h>

#include <gtest/gtest.h>

#include <takatori/relation/values.h>
#include <takatori/relation/write.h>

#include <takatori/plan/process.h>
#include <takatori/plan/forward.h>

#include <takatori/statement/execute.h>
#include <takatori/statement/write.h>

#include <yugawara/storage/configurable_provider.h>
#include <yugawara/binding/factory.h>

#include <yugawara/restricted_feature.h>

#include <yugawara/testing/utils.h>

namespace yugawara::details {

// import test utils
using namespace ::yugawara::testing;

class collect_restricted_features_test : public ::testing::Test {
protected:
    binding::factory factory;

    std::shared_ptr<storage::configurable_provider> storages = std::make_shared<storage::configurable_provider>();
    std::shared_ptr<analyzer::index_estimator> indices {};

    std::shared_ptr<storage::table> t0 = storages->add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];
    storage::column const& t0c2 = t0->columns()[2];

    std::shared_ptr<storage::index> i0 = storages->add_index({ t0, "I0", });

    void check(diagnostic<compiler_code> const& d, restricted_feature f) {
        EXPECT_EQ(d.code(), compiler_code::unsupported_feature);
        EXPECT_NE(d.message().find(std::string { to_string_view(f) }), std::string::npos) << d.message();
    }

    ::takatori::statement::execute make_statement(relation::expression&& expression) {
        ::takatori::relation::graph_type operators {};
        operators.insert(std::move(expression));

        ::takatori::plan::graph_type steps {};
        steps.insert(plan::process { std::move(operators) });

        return ::takatori::statement::execute {
                std::move(steps),
        };
    }

    ::takatori::statement::execute make_statement(plan::step&& step) {
        ::takatori::plan::graph_type steps {};
        steps.insert(std::move(step));

        return ::takatori::statement::execute {
                std::move(steps),
        };
    }
};

TEST_F(collect_restricted_features_test, relation_values_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::relation_values },
            make_statement(relation::values {
                    {},
                    {},
            }));
    ASSERT_EQ(result.size(), 1);
    check(result[0], restricted_feature::relation_values);
}

TEST_F(collect_restricted_features_test, relation_values_not_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::relation_buffer },
            make_statement(relation::values {
                    {},
                    {},
            }));
    ASSERT_EQ(result.size(), 0);
}

TEST_F(collect_restricted_features_test, relation_write_insert_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::relation_write_insert },
            make_statement(relation::write {
                    relation::write_kind::insert,
                    factory(*i0),
                    {},
                    {},
            }));
    ASSERT_EQ(result.size(), 1);
    check(result[0], restricted_feature::relation_write_insert);
}

TEST_F(collect_restricted_features_test, relation_write_insert_overwrite_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::relation_write_insert },
            make_statement(relation::write {
                    relation::write_kind::insert_overwrite,
                    factory(*i0),
                    {},
                    {},
            }));
    ASSERT_EQ(result.size(), 1);
    check(result[0], restricted_feature::relation_write_insert);
}

TEST_F(collect_restricted_features_test, relation_write_insert_skip_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::relation_write_insert },
            make_statement(relation::write {
                    relation::write_kind::insert_skip,
                    factory(*i0),
                    {},
                    {},
            }));
    ASSERT_EQ(result.size(), 1);
    check(result[0], restricted_feature::relation_write_insert);
}

TEST_F(collect_restricted_features_test, relation_write_insert_not_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::relation_write_insert },
            make_statement(relation::write {
                    relation::write_kind::update,
                    factory(*i0),
                    {},
                    {},
            }));
    ASSERT_EQ(result.size(), 0);
}

TEST_F(collect_restricted_features_test, exchange_forward_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::exchange_forward },
            make_statement(plan::forward {}));
    ASSERT_EQ(result.size(), 1);
    check(result[0], restricted_feature::exchange_forward);
}

TEST_F(collect_restricted_features_test, exchange_forward_not_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::exchange_group },
            make_statement(plan::forward {}));
    ASSERT_EQ(result.size(), 0);
}

TEST_F(collect_restricted_features_test, statement_write_update_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::statement_write_update },
            ::takatori::statement::write {
                    ::takatori::statement::write_kind::update,
                    factory(*i0),
                    {},
                    {},
            });
    ASSERT_EQ(result.size(), 1);
    check(result[0], restricted_feature::statement_write_update);
}

TEST_F(collect_restricted_features_test, statement_write_delete_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::statement_write_delete },
            ::takatori::statement::write {
                    ::takatori::statement::write_kind::delete_,
                    factory(*i0),
                    {},
                    {},
            });
    ASSERT_EQ(result.size(), 1);
    check(result[0], restricted_feature::statement_write_delete);
}

TEST_F(collect_restricted_features_test, statement_write_update_not_restricted) {
    auto result = collect_restricted_features(
            { restricted_feature::statement_write_update },
            make_statement(relation::write {
                    relation::write_kind::update,
                    factory(*i0),
                    {},
                    {},
            }));
    ASSERT_EQ(result.size(), 0);
}

} // namespace yugawara::details
