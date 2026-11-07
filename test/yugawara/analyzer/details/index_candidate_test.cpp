#include <yugawara/analyzer/details/index_candidate.h>

#include <gtest/gtest.h>

#include <yugawara/storage/configurable_provider.h>

#include <yugawara/testing/utils.h>

namespace yugawara::analyzer::details {

using namespace ::yugawara::testing;

class index_candidate_test: public ::testing::Test {
protected:
    using kind = index_candidate::access_kind;
    using result = index_estimator::result;

    storage::configurable_provider storages;

    std::shared_ptr<storage::table> t0 = storages.add_table({
            "T0",
            {
                    { "C0", t::int4() },
            },
    });

    std::shared_ptr<storage::index> ia = storages.add_index({ t0, "A", });
    std::shared_ptr<storage::index> ib = storages.add_index({ t0, "B", });

    index_selection_parameters parameters {};

    static index_candidate candidate(storage::index const& index, result estimation, bool direct) {
        return index_candidate {
                index,
                estimation,
                {},
                kind::range_scan,
                true,
                direct,
        };
    }

    [[nodiscard]] candidate_comparison compare(index_candidate const& first, index_candidate const& second) const {
        return compare_index_candidates(first, second, parameters);
    }
};

TEST_F(index_candidate_test, single_row) {
    auto single = candidate(*ia, result { 0.1, 1 }, false);
    auto other = candidate(*ib, result { 0.01 }, true);
    EXPECT_TRUE(is_single_row(single));
    EXPECT_FALSE(is_single_row(other));
    EXPECT_EQ(compare(single, other), candidate_comparison::better);
    EXPECT_EQ(compare(other, single), candidate_comparison::worse);
}

TEST_F(index_candidate_test, selectivity) {
    auto narrow = candidate(*ia, result { 0.1 }, true);
    auto wide = candidate(*ib, result { 0.6 }, true);
    EXPECT_EQ(compare(narrow, wide), candidate_comparison::better);
    EXPECT_EQ(compare(wide, narrow), candidate_comparison::worse);
}

TEST_F(index_candidate_test, indirect_weight) {
    // 0.6 * 2 > 1.0
    auto indirect = candidate(*ia, result { 0.6 }, false);
    auto direct = candidate(*ib, result { 1.0 }, true);
    EXPECT_EQ(compare(indirect, direct), candidate_comparison::worse);

    // 0.3 * 2 < 1.0
    auto selective = candidate(*ia, result { 0.3 }, false);
    EXPECT_EQ(compare(selective, direct), candidate_comparison::better);
}

TEST_F(index_candidate_test, indirect_weight_parameter) {
    parameters.indirect_access_weight() = 1.5;
    // 0.6 * 1.5 < 1.0
    auto indirect = candidate(*ia, result { 0.6 }, false);
    auto direct = candidate(*ib, result { 1.0 }, true);
    EXPECT_EQ(compare(indirect, direct), candidate_comparison::better);
}

TEST_F(index_candidate_test, equivalent) {
    // 0.3 * 2 == 0.6
    auto indirect = candidate(*ia, result { 0.3 }, false);
    auto direct = candidate(*ib, result { 0.6 }, true);
    EXPECT_EQ(compare(indirect, direct), candidate_comparison::equivalent);
}

TEST_F(index_candidate_test, count) {
    auto many = candidate(*ia, result { 0.1, 100 }, true);
    auto few = candidate(*ib, result { 0.6, 10 }, true);
    EXPECT_EQ(compare(few, many), candidate_comparison::better);
}

TEST_F(index_candidate_test, count_partial) {
    // compares the selectivity if only one of them has the count
    auto counted = candidate(*ia, result { 0.6, 10 }, true);
    auto other = candidate(*ib, result { 0.1 }, true);
    EXPECT_EQ(compare(other, counted), candidate_comparison::better);
}

TEST_F(index_candidate_test, scale) {
    auto first = candidate(*ia, result { 0.1 }, true);
    auto second = candidate(*ib, result { 0.6 }, true);
    EXPECT_EQ(compare_index_candidates(first, 1.0, second, 10.0, parameters), candidate_comparison::worse);
}

TEST_F(index_candidate_test, scan_candidate_prefers_direct) {
    auto indirect = candidate(*ia, result { 0.3 }, false);
    auto direct = candidate(*ib, result { 0.6 }, true);
    EXPECT_TRUE(is_better_scan_candidate(direct, indirect, parameters));
    EXPECT_FALSE(is_better_scan_candidate(indirect, direct, parameters));
}

TEST_F(index_candidate_test, scan_candidate_prefers_name) {
    auto a = candidate(*ia, result { 0.1 }, true);
    auto b = candidate(*ib, result { 0.1 }, true);
    EXPECT_TRUE(is_better_scan_candidate(a, b, parameters));
    EXPECT_FALSE(is_better_scan_candidate(b, a, parameters));
}

} // namespace yugawara::analyzer::details
