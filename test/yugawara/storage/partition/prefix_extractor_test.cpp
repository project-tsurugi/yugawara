#include <yugawara/storage/partition/prefix_extractor.h>

#include <gtest/gtest.h>

namespace yugawara::storage::partition {

class prefix_extractor_test : public ::testing::Test {};

TEST_F(prefix_extractor_test, nothing) {
    prefix_extractor e {};
    EXPECT_EQ(e.kind(), prefix_extractor_kind::nothing);
    EXPECT_FALSE(e);
}

TEST_F(prefix_extractor_test, character_prefix) {
    prefix_extractor e { character_prefix { 3 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::character_prefix);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::character_prefix>(), character_prefix { 3 });
}

TEST_F(prefix_extractor_test, binary_prefix) {
    prefix_extractor e { binary_prefix { 4 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::binary_prefix);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::binary_prefix>(), binary_prefix { 4 });
}

TEST_F(prefix_extractor_test, floor_division) {
    prefix_extractor e { floor_division { 1000 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::floor_division);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::floor_division>(), floor_division { 1000 });
}

TEST_F(prefix_extractor_test, floor_fractional) {
    prefix_extractor e { floor_fractional { 2 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::floor_fractional);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::floor_fractional>(), floor_fractional { 2 });
}

TEST_F(prefix_extractor_test, date_truncate) {
    prefix_extractor e { date_truncate { date_unit::month } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::date_truncate);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::date_truncate>(), date_truncate { date_unit::month });
}

TEST_F(prefix_extractor_test, timestamp_truncate) {
    prefix_extractor e { timestamp_truncate { timestamp_unit::second, 3 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::timestamp_truncate);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::timestamp_truncate>(), (timestamp_truncate { timestamp_unit::second, 3 }));
}

TEST_F(prefix_extractor_test, equals) {
    EXPECT_EQ(prefix_extractor {}, prefix_extractor {});
    EXPECT_EQ(prefix_extractor { character_prefix { 3 } }, prefix_extractor { character_prefix { 3 } });
    EXPECT_NE(prefix_extractor { character_prefix { 3 } }, prefix_extractor { character_prefix { 4 } });
    EXPECT_NE(prefix_extractor { character_prefix { 3 } }, prefix_extractor { binary_prefix { 3 } });
    EXPECT_NE(prefix_extractor { character_prefix { 3 } }, prefix_extractor {});
}

TEST_F(prefix_extractor_test, output) {
    std::cout << prefix_extractor {} << std::endl;
    std::cout << prefix_extractor { character_prefix { 3 } } << std::endl;
    std::cout << prefix_extractor { binary_prefix { 4 } } << std::endl;
    std::cout << prefix_extractor { floor_division { 1000 } } << std::endl;
    std::cout << prefix_extractor { floor_fractional { 2 } } << std::endl;
    std::cout << prefix_extractor { date_truncate { date_unit::month } } << std::endl;
    std::cout << prefix_extractor { timestamp_truncate { timestamp_unit::second, 3 } } << std::endl;
}

} // namespace yugawara::storage::partition
