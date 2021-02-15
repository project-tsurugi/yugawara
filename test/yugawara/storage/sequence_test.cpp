#include <yugawara/storage/sequence.h>

#include <gtest/gtest.h>

namespace yugawara::storage {

class sequence_test : public ::testing::Test {};

TEST_F(sequence_test, simple) {
    sequence t { "S1" };

    EXPECT_FALSE(t.definition_id());
    EXPECT_EQ(t.simple_name(), "S1");
    EXPECT_EQ(t.initial_value(), sequence::default_initial_value);
    EXPECT_EQ(t.increment_value(), sequence::default_increment_value);
    EXPECT_EQ(t.min_value(), sequence::default_min_value);
    EXPECT_EQ(t.max_value(), sequence::default_max_value);
    EXPECT_EQ(t.cycle(), sequence::default_cycle);
}

TEST_F(sequence_test, properties) {
    sequence t {
        1,
        "S1",
        2,
        100,
        10,
        1'000,
        false,
    };

    EXPECT_EQ(t.simple_name(), "S1");
    EXPECT_EQ(t.definition_id(), 1);
    EXPECT_EQ(t.initial_value(), 2);
    EXPECT_EQ(t.increment_value(), 100);
    EXPECT_EQ(t.min_value(), 10);
    EXPECT_EQ(t.max_value(), 1'000);
    EXPECT_EQ(t.cycle(), false);
}

TEST_F(sequence_test, output) {
    sequence t {
            1,
            "S1",
            2,
            100,
            10,
            1'000,
            false,
    };
    std::cout << t << std::endl;
}

} // namespace yugawara::storage
