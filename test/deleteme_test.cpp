#include <gtest/gtest.h>

namespace yugawara {

class deleteme_test : public ::testing::Test {};

TEST_F(deleteme_test, simple) {
    std::cout << "DELETEME" << std::endl;
}

} // namespace yugawara
