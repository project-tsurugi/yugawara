#include <yugawara/variable/configurable_provider.h>

#include <gtest/gtest.h>

#include <takatori/type/character.h>

namespace yugawara::variable {

class variable_configurable_provider_test : public ::testing::Test {};

using c = takatori::type::character;

TEST_F(variable_configurable_provider_test, add) {
    auto p1 = std::make_shared<configurable_provider>();

    auto v1 = p1->add({ "v1", c(1), });

    EXPECT_THROW(p1->add({ "v1", c(2), }), std::invalid_argument);

    auto f1 = p1->find("v1");
    EXPECT_EQ(f1, v1);
}

TEST_F(variable_configurable_provider_test, add_overwrite) {
    auto p1 = std::make_shared<configurable_provider>();

    auto v1 = p1->add({ "v1", c(1), });
    auto v2 = p1->add({ "v1", c(2), }, true);

    EXPECT_THROW(p1->add({ "v1", c(2), }), std::invalid_argument);

    auto f1 = p1->find("v1");
    EXPECT_EQ(f1, v2);
}

TEST_F(variable_configurable_provider_test, add_hide) {
    auto p1 = std::make_shared<configurable_provider>();
    auto p2 = std::make_shared<configurable_provider>(p1);

    auto v1 = p1->add({ "v1", c(1), });
    EXPECT_THROW(p2->add({ "v1", c(2), }), std::invalid_argument);

    auto v2 = p2->add({ "v1", c(2), }, true);

    auto f1 = p1->find("v1");
    EXPECT_EQ(f1, v1);

    auto f2 = p2->find("v1");
    EXPECT_EQ(f2, v2);
}

TEST_F(variable_configurable_provider_test, remove) {
    auto p1 = std::make_shared<configurable_provider>();

    auto v1 = p1->add({ "v1", c(1), });
    auto v2 = p1->add({ "v2", c(2), });
    auto v3 = p1->add({ "v3", c(3), });

    EXPECT_TRUE(p1->remove(*v2));

    std::vector<declaration const*> v;
    p1->each([&](auto& p) { v.emplace_back(p.get()); });

    ASSERT_EQ(v.size(), 2);
    EXPECT_EQ(v[0], v1.get());
    EXPECT_EQ(v[1], v3.get());

    EXPECT_FALSE(p1->remove(*v2));
}

TEST_F(variable_configurable_provider_test, each) {
    auto p1 = std::make_shared<configurable_provider>();

    auto v1 = p1->add({ "v1", c(1), });
    auto v2 = p1->add({ "v2", c(2), });
    auto v3 = p1->add({ "v3", c(3), });

    std::vector<declaration const*> v;
    p1->each([&](auto& p) { v.emplace_back(p.get()); });

    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], v1.get());
    EXPECT_EQ(v[1], v2.get());
    EXPECT_EQ(v[2], v3.get());
}

TEST_F(variable_configurable_provider_test, each_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto p2 = std::make_shared<configurable_provider>(p1);

    auto v11 = p1->add({ "v1", c(1), });
    auto v12 = p1->add({ "v2", c(2), });

    auto v22 = p2->add({ "v2", c(3), }, true);
    auto v23 = p2->add({ "v3", c(4), }, true);

    std::vector<declaration const*> v;
    p2->each([&](auto& p) { v.emplace_back(p.get()); });
    std::sort(v.begin(), v.end(), [](auto* a, auto* b) {
        return a->name() < b->name();
    });

    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], v11.get());
    EXPECT_EQ(v[1], v22.get());
    EXPECT_EQ(v[2], v23.get());
}

} // namespace yugawara::variable
