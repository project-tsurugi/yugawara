#include <yugawara/schema/configurable_provider.h>

#include <gtest/gtest.h>

namespace yugawara::schema {

class schema_configurable_provider_test : public ::testing::Test {};

TEST_F(schema_configurable_provider_test, add) {
    configurable_provider p;
    auto s = p.add({ "s", 1 });

    EXPECT_THROW(p.add({ "s", 2 }), std::invalid_argument);

    auto r = p.find("s");
    EXPECT_EQ(r, s);
}

TEST_F(schema_configurable_provider_test, add_overwrite) {
    configurable_provider p;
    auto s1 = p.add({ "s", 1 });
    auto s2 = p.add({ "s", 2 }, true);

    auto r = p.find("s");
    EXPECT_EQ(r, s2);
}

TEST_F(schema_configurable_provider_test, remove) {
    configurable_provider p;
    auto s0 = p.add({ "a", 0 });
    auto s1 = p.add({ "b", 1 });
    auto s2 = p.add({ "c", 2 });

    EXPECT_TRUE(p.remove(*s1));

    std::vector<declaration const*> v;
    p.each([&](std::shared_ptr<declaration> const& p) { v.emplace_back(p.get()); });

    ASSERT_EQ(v.size(), 2);
    EXPECT_EQ(v[0], s0.get());
    EXPECT_EQ(v[1], s2.get());

    EXPECT_FALSE(p.remove(*s1));
}

TEST_F(schema_configurable_provider_test, each) {
    configurable_provider p;
    auto s0 = p.add({ "a", 0 });
    auto s1 = p.add({ "b", 1 });
    auto s2 = p.add({ "c", 2 });

    std::vector<declaration*> v;
    p.each([&](std::shared_ptr<declaration> const& p) { v.emplace_back(p.get()); });

    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], s0.get());
    EXPECT_EQ(v[1], s1.get());
    EXPECT_EQ(v[2], s2.get());
}

TEST_F(schema_configurable_provider_test, each_const) {
    configurable_provider p;
    auto s0 = p.add({ "a", 0 });
    auto s1 = p.add({ "b", 1 });
    auto s2 = p.add({ "c", 2 });

    std::vector<declaration const*> v;
    p.each([&](std::shared_ptr<declaration const> const& p) { v.emplace_back(p.get()); });

    ASSERT_EQ(v.size(), 3);
    EXPECT_EQ(v[0], s0.get());
    EXPECT_EQ(v[1], s1.get());
    EXPECT_EQ(v[2], s2.get());
}

} // namespace yugawara::schema
