#include <yugawara/analyzer/details/row_slice_utils.h>

#include <gtest/gtest.h>

#include <yugawara/binding/factory.h>

namespace yugawara::analyzer::details {

using ::takatori::relation::details::row_slice;

class row_slice_utils_test : public ::testing::Test {
protected:
    binding::factory bindings;
};

TEST_F(row_slice_utils_test, has_start) {
    EXPECT_FALSE(has_start(row_slice {}));
    EXPECT_FALSE(has_start(row_slice { {}, 3 }));
    EXPECT_FALSE(has_start(row_slice { 0, 3 }));
    EXPECT_TRUE(has_start(row_slice { 1, 3 }));
    EXPECT_TRUE(has_start(row_slice { 1, {} }));
}

TEST_F(row_slice_utils_test, has_start_variable) {
    auto n = bindings.stream_variable("n");
    EXPECT_TRUE(has_start(row_slice { n, 3 }));
}

TEST_F(row_slice_utils_test, merge_row_slice_immediate) {
    EXPECT_EQ(merge_row_slice(row_slice { {}, 3 }, row_slice { {}, 5 }), (row_slice { {}, 3 }));
    EXPECT_EQ(merge_row_slice(row_slice { {}, 5 }, row_slice { {}, 3 }), (row_slice { {}, 3 }));
    EXPECT_EQ(merge_row_slice(row_slice { {}, 3 }, row_slice { {}, 3 }), (row_slice { {}, 3 }));
}

TEST_F(row_slice_utils_test, merge_row_slice_absent) {
    EXPECT_EQ(merge_row_slice(row_slice {}, row_slice { {}, 3 }), (row_slice { {}, 3 }));
    EXPECT_EQ(merge_row_slice(row_slice { {}, 3 }, row_slice {}), (row_slice { {}, 3 }));
    EXPECT_EQ(merge_row_slice(row_slice {}, row_slice {}), (row_slice {}));
}

TEST_F(row_slice_utils_test, merge_row_slice_zero_start) {
    EXPECT_EQ(merge_row_slice(row_slice { 0, 3 }, row_slice { {}, 5 }), (row_slice { {}, 3 }));
}

TEST_F(row_slice_utils_test, merge_row_slice_start) {
    EXPECT_FALSE(merge_row_slice(row_slice { 1, 3 }, row_slice { {}, 5 }));
    EXPECT_FALSE(merge_row_slice(row_slice { {}, 3 }, row_slice { 1, 5 }));
}

TEST_F(row_slice_utils_test, merge_row_slice_variable) {
    auto n = bindings.stream_variable("n");
    auto m = bindings.stream_variable("m");
    EXPECT_EQ(merge_row_slice(row_slice { {}, n }, row_slice { {}, n }), (row_slice { {}, n }));
    EXPECT_EQ(merge_row_slice(row_slice {}, row_slice { {}, n }), (row_slice { {}, n }));
    EXPECT_EQ(merge_row_slice(row_slice { {}, n }, row_slice {}), (row_slice { {}, n }));
    EXPECT_FALSE(merge_row_slice(row_slice { {}, n }, row_slice { {}, m }));
    EXPECT_FALSE(merge_row_slice(row_slice { {}, n }, row_slice { {}, 3 }));
    EXPECT_FALSE(merge_row_slice(row_slice { {}, 3 }, row_slice { {}, n }));
}

} // namespace yugawara::analyzer::details
