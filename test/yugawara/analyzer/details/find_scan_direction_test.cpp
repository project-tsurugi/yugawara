#include <yugawara/analyzer/details/find_scan_direction.h>

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

class find_scan_direction_test : public ::testing::Test {
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

    static std::optional<scan_direction> apply(
            storage::index const& index,
            std::vector<key> const& sort_keys,
            std::vector<column_ref> const& fixed_columns = {}) {
        return find_scan_direction(index, sort_keys, fixed_columns);
    }
};

TEST_F(find_scan_direction_test, primary_prefix) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, asc } }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, primary_all) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, asc }, { b, asc } }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, primary_after_unique_keys) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, asc }, { b, asc }, { c, desc } }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, descendant_key) {
    auto&& index = primary({ { a, desc } });
    EXPECT_EQ(apply(index, { { a, desc } }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, direction_reversed) {
    auto&& index = primary({ a, b });
    EXPECT_NE(apply(index, { { a, desc } }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, direction_reversed_descendant_key) {
    auto&& index = primary({ { a, desc } });
    EXPECT_NE(apply(index, { { a, asc } }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, direction_mixed) {
    auto&& index = primary({ a, b });
    EXPECT_FALSE(apply(index, { { a, asc }, { b, desc } }));
}

TEST_F(find_scan_direction_test, not_prefix) {
    auto&& index = primary({ a, b });
    EXPECT_FALSE(apply(index, { { b, asc } }));
}

TEST_F(find_scan_direction_test, fixed_index_key) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { b, asc } }, { a }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, fixed_sort_key) {
    auto&& index = primary({ a, b });
    EXPECT_EQ(apply(index, { { a, desc }, { b, asc } }, { a }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, fixed_not_prefix) {
    auto&& index = primary({ a, b, c });
    EXPECT_FALSE(apply(index, { { c, asc } }, { a }));
}

TEST_F(find_scan_direction_test, secondary_prefix) {
    auto&& index = secondary({ v });
    EXPECT_EQ(apply(index, { { v, asc } }), scan_direction::forward);
}

TEST_F(find_scan_direction_test, secondary_never_assume_primary_keys) {
    primary({ a });
    auto&& index = secondary({ v });
    EXPECT_FALSE(apply(index, { { v, asc }, { a, asc } }));
}

TEST_F(find_scan_direction_test, unique_secondary_never_omit_rest) {
    primary({ a });
    auto&& index = secondary({ v }, true);
    EXPECT_FALSE(apply(index, { { v, asc }, { a, asc } }));
}

} // namespace yugawara::analyzer::details
