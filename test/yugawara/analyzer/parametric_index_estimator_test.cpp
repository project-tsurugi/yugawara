#include <yugawara/analyzer/parametric_index_estimator.h>

#include <gtest/gtest.h>

#include <cmath>

#include <takatori/scalar/immediate.h>

#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer {

using namespace ::yugawara::testing;

class parametric_index_estimator_test: public ::testing::Test {
protected:
    using search_key = index_estimator::search_key;
    using semantics = ::takatori::relation::comparison_semantics_kind;

    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int4() },
                    { "C1", t::int4() },
                    { "C2", t::int4() },
            },
    });
    storage::column const& t0c0 = t0->columns()[0];
    storage::column const& t0c1 = t0->columns()[1];

    std::shared_ptr<storage::index> unique_index = storages.add_index(storage::index {
            t0,
            "U",
            { t0c0, t0c1 },
            {},
            {
                    storage::index_feature::find,
                    storage::index_feature::scan,
                    storage::index_feature::unique,
            },
    });

    std::shared_ptr<storage::index> non_unique_index = storages.add_index(storage::index {
            t0,
            "N",
            { t0c0, t0c1 },
            {},
            {
                    storage::index_feature::find,
                    storage::index_feature::scan,
            },
    });

    ::takatori::scalar::immediate v0 = constant(0);
    ::takatori::scalar::immediate v1 = constant(1);

    search_key equivalent(storage::column const& column) {
        return search_key { column, v0, semantics::ternary };
    }

    search_key half_bound(storage::column const& column) {
        return search_key { column, v0, true, {}, false };
    }

    search_key full_bound(storage::column const& column) {
        return search_key { column, v0, true, v1, false };
    }

    static index_estimator::result estimate(
            index_estimator const& estimator,
            storage::index const& index,
            std::vector<search_key> const& keys) {
        return estimator(index, keys);
    }
};

TEST_F(parametric_index_estimator_test, full_scan) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *unique_index, {});
    EXPECT_DOUBLE_EQ(result.selectivity(), 1.0);
    EXPECT_FALSE(result.count());
}

TEST_F(parametric_index_estimator_test, equivalent) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *non_unique_index, { equivalent(t0c0) });
    EXPECT_DOUBLE_EQ(result.selectivity(), 0.1);
    EXPECT_FALSE(result.count());
}

TEST_F(parametric_index_estimator_test, half_bound) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *non_unique_index, { half_bound(t0c0) });
    EXPECT_DOUBLE_EQ(result.selectivity(), 0.6);
}

TEST_F(parametric_index_estimator_test, full_bound) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *non_unique_index, { full_bound(t0c0) });
    EXPECT_DOUBLE_EQ(result.selectivity(), 0.3);
}

TEST_F(parametric_index_estimator_test, prefix_then_range) {
    parametric_index_estimator estimator {};
    auto narrow = estimate(estimator, *non_unique_index, { equivalent(t0c0), half_bound(t0c1) });
    auto wide = estimate(estimator, *non_unique_index, { half_bound(t0c0) });
    EXPECT_DOUBLE_EQ(narrow.selectivity(), 0.1 * std::pow(0.6, 0.5));
    EXPECT_LT(narrow.selectivity(), wide.selectivity());
}

TEST_F(parametric_index_estimator_test, unique_point) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *unique_index, { equivalent(t0c0), equivalent(t0c1) });
    EXPECT_DOUBLE_EQ(result.selectivity(), 0.1 * std::pow(0.1, 0.5));
    EXPECT_EQ(result.count(), 1);
}

TEST_F(parametric_index_estimator_test, unique_prefix) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *unique_index, { equivalent(t0c0) });
    EXPECT_FALSE(result.count());
}

TEST_F(parametric_index_estimator_test, unique_range) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *unique_index, { equivalent(t0c0), half_bound(t0c1) });
    EXPECT_FALSE(result.count());
}

TEST_F(parametric_index_estimator_test, non_unique_point) {
    parametric_index_estimator estimator {};
    auto result = estimate(estimator, *non_unique_index, { equivalent(t0c0), equivalent(t0c1) });
    EXPECT_FALSE(result.count());
}

TEST_F(parametric_index_estimator_test, parameters) {
    parametric_index_estimator::parameters_type parameters {};
    parameters.equivalent_selectivity() = 0.5;
    parameters.full_bound_selectivity() = 0.25;
    parameters.half_bound_selectivity() = 0.75;
    parametric_index_estimator estimator { parameters };

    EXPECT_DOUBLE_EQ(estimate(estimator, *non_unique_index, { equivalent(t0c0) }).selectivity(), 0.5);
    EXPECT_DOUBLE_EQ(estimate(estimator, *non_unique_index, { full_bound(t0c0) }).selectivity(), 0.25);
    EXPECT_DOUBLE_EQ(estimate(estimator, *non_unique_index, { half_bound(t0c0) }).selectivity(), 0.75);
}

TEST_F(parametric_index_estimator_test, decay_none) {
    parametric_index_estimator::parameters_type parameters {};
    parameters.selectivity_decay() = 1.0;
    parametric_index_estimator estimator { parameters };
    auto result = estimate(estimator, *non_unique_index, { equivalent(t0c0), half_bound(t0c1) });
    EXPECT_DOUBLE_EQ(result.selectivity(), 0.1 * 0.6);
}

TEST_F(parametric_index_estimator_test, decay_first_only) {
    parametric_index_estimator::parameters_type parameters {};
    parameters.selectivity_decay() = 0.0;
    parametric_index_estimator estimator { parameters };
    auto result = estimate(estimator, *non_unique_index, { equivalent(t0c0), half_bound(t0c1) });
    EXPECT_DOUBLE_EQ(result.selectivity(), 0.1);
}

TEST_F(parametric_index_estimator_test, decay_default) {
    parametric_index_estimator estimator {};
    auto one = estimate(estimator, *non_unique_index, { equivalent(t0c0) });
    auto two = estimate(estimator, *non_unique_index, { equivalent(t0c0), equivalent(t0c1) });
    EXPECT_DOUBLE_EQ(two.selectivity(), 0.1 * std::pow(0.1, 0.5));
    // still decreases monotonically
    EXPECT_LT(two.selectivity(), one.selectivity());
}

} // namespace yugawara::analyzer
