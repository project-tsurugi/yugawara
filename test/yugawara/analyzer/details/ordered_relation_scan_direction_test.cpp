#include <yugawara/analyzer/details/ordered_relation.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>

#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

// import test utils
using namespace ::yugawara::testing;

using ::takatori::relation::scan_direction;

using key = storage::details::index_key_element;
using column_ref = std::reference_wrapper<storage::column const>;

class ordered_relation_scan_direction_test : public ::testing::Test {
protected:
    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "A", t::int4() },
                    { "B", t::int4() },
                    { "C", t::int4() },
                    { "V", t::int4() },
            },
    });
    storage::column const& a = t0->columns()[0];
    storage::column const& b = t0->columns()[1];
    storage::column const& c = t0->columns()[2];
    storage::column const& v = t0->columns()[3];

    static constexpr auto asc = storage::sort_direction::ascendant;
    static constexpr auto desc = storage::sort_direction::descendant;

    storage::index const& primary(std::initializer_list<key> keys) {
        return *storages.add_index({
                t0,
                "P",
                keys,
                {},
                {
                        storage::index_feature::primary,
                        storage::index_feature::find,
                        storage::index_feature::scan,
                        storage::index_feature::unique,
                },
        });
    }

    storage::index const& secondary(std::initializer_list<key> keys, bool unique = false) {
        storage::index_feature_set features {
                storage::index_feature::find,
                storage::index_feature::scan,
        };
        if (unique) {
            features.insert(storage::index_feature::unique);
        }
        return *storages.add_index({
                t0,
                "S",
                keys,
                {},
                features,
        });
    }

    storage::index const& ties_ordered(std::initializer_list<key> keys) {
        return *storages.add_index({
                t0,
                "S",
                keys,
                {},
                {
                        storage::index_feature::find,
                        storage::index_feature::scan,
                        storage::index_feature::ties_ordered_by_primary_key,
                },
        });
    }

    static std::optional<scan_direction> apply(
            storage::index const& index,
            std::vector<key> const& sort_keys,
            std::vector<column_ref> const& fixed_columns = {}) {
        return find_scan_direction(index, sort_keys, fixed_columns);
    }
};

TEST_F(ordered_relation_scan_direction_test, primary_prefix) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, asc } }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, primary_all) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, asc }, { b, asc } }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, primary_after_unique_keys) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, asc }, { b, asc }, { c, desc } }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, descendant_key) {
    auto&& index = primary({ { a, desc } });
    EXPECT_EQ(apply(index, { { a, desc } }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, direction_reversed) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, desc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, direction_reversed_descendant_key) {
    auto&& index = primary({ { a, desc } });
    EXPECT_EQ(apply(index, { { a, asc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, direction_reversed_all) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, desc }, { b, desc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, direction_reversed_mixed_keys) {
    auto&& index = primary({ a, { b, desc } });
    EXPECT_EQ(apply(index, { { a, asc }, { b, desc } }), scan_direction::forward);
    EXPECT_EQ(apply(index, { { a, desc }, { b, asc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, direction_reversed_after_unique_keys) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, desc }, { b, desc }, { c, asc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, direction_mixed) {
    auto&& index = primary({ a, b });
    EXPECT_FALSE(apply(index, { { a, asc }, { b, desc } }));
}

TEST_F(ordered_relation_scan_direction_test, direction_mixed_reversed_first) {
    auto&& index = primary({ a, b });
    EXPECT_FALSE(apply(index, { { a, desc }, { b, asc } }));
}

TEST_F(ordered_relation_scan_direction_test, direction_reversed_fixed_index_key) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { b, desc } }, { a }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, direction_reversed_fixed_sort_key) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, asc }, { b, desc } }, { a }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, all_fixed_prefers_forward) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, desc } }, { a }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, not_prefix) {
    auto&& index = primary({ a, b });
    EXPECT_FALSE(apply(index, { { b, asc } }));
}

TEST_F(ordered_relation_scan_direction_test, fixed_index_key) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { b, asc } }, { a }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, fixed_sort_key) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, desc }, { b, asc } }, { a }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, fixed_not_prefix) {
    auto&& index = primary({ a, b, c });
    EXPECT_FALSE(apply(index, { { c, asc } }, { a }));
}

TEST_F(ordered_relation_scan_direction_test, secondary_prefix) {
    auto&& index = secondary({ v });
    EXPECT_EQ(apply(index, { { v, asc } }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, secondary_reversed) {
    // NOTE: the column may contain nulls, but they are the smallest values in both the index keys and the sort keys
    auto&& index = secondary({ v });
    EXPECT_EQ(apply(index, { { v, desc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, secondary_descendant_key_reversed) {
    auto&& index = secondary({ { v, desc } });
    EXPECT_EQ(apply(index, { { v, asc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, secondary_never_assume_primary_keys) {
    primary({ a });
    auto&& index = secondary({ v });
    EXPECT_FALSE(apply(index, { { v, asc }, { a, asc } }));
}

TEST_F(ordered_relation_scan_direction_test, ties_primary_key_follows) {
    primary({ a });
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { v, a }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, ties_reversed) {
    primary({ a });
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { { v, desc }, { a, desc } }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, ties_direction_mixed) {
    primary({ a });
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { v, { a, desc } }), std::nullopt);
}

TEST_F(ordered_relation_scan_direction_test, ties_descendant_primary_key) {
    primary({ { a, desc } });
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { v, { a, desc } }), scan_direction::forward);
    EXPECT_EQ(apply(index, { { v, desc }, a }), scan_direction::backward);
}

TEST_F(ordered_relation_scan_direction_test, ties_fixed_index_key) {
    primary({ a });
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { a }, { v }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, ties_after_primary_key) {
    primary({ a });
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { v, a, c }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, ties_primary_key_prefix) {
    primary({ a, b });
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { v, a }), scan_direction::forward);
    EXPECT_EQ(apply(index, { v, b }), std::nullopt);
}

TEST_F(ordered_relation_scan_direction_test, ties_primary_key_in_index_key) {
    primary({ a, b });
    auto&& index = ties_ordered({ b });
    EXPECT_EQ(apply(index, { b, a }), scan_direction::forward);
}

TEST_F(ordered_relation_scan_direction_test, ties_without_primary_index) {
    auto&& index = ties_ordered({ v });
    EXPECT_EQ(apply(index, { v }), scan_direction::forward);
    EXPECT_EQ(apply(index, { v, a }), std::nullopt);
}

TEST_F(ordered_relation_scan_direction_test, unique_secondary_never_omit_rest) {
    primary({ a });
    auto&& index = secondary({ v }, true);
    EXPECT_FALSE(apply(index, { { v, asc }, { a, asc } }));
}

} // namespace yugawara::analyzer::details
