#include <yugawara/storage/sequence.h>

#include <gtest/gtest.h>

namespace yugawara::storage {

class sequence_test : public ::testing::Test {};

TEST_F(sequence_test, simple) {
    sequence t { "S1" };

    EXPECT_FALSE(t.definition_id());
    EXPECT_EQ(t.simple_name(), "S1");
    ASSERT_EQ(t.min_value(), sequence::default_min_value);
    ASSERT_EQ(t.max_value(), sequence::default_max_value);
    ASSERT_EQ(t.increment_value(), sequence::default_increment_value);
}

TEST_F(sequence_test, properties) {
    sequence t {
        1,
        "S1",
        100,
        1'000,
        10,
    };

    EXPECT_EQ(t.simple_name(), "S1");
    EXPECT_EQ(t.definition_id(), 1);
    ASSERT_EQ(t.min_value(), 100);
    ASSERT_EQ(t.max_value(), 1'000);
    ASSERT_EQ(t.increment_value(), 10);
}

TEST_F(sequence_test, output) {
    sequence t {
            1,
            "S1",
            100,
            1'000,
            10,
    };
    std::cout << t << std::endl;
}

} // namespace yugawara::storage
