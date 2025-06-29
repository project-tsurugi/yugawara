#include <yugawara/aggregate/configurable_provider.h>

#include <gtest/gtest.h>

#include <algorithm>

#include <takatori/type/primitive.h>
#include <takatori/type/character.h>

namespace yugawara::aggregate {

class aggregate_configurable_provider_test : public ::testing::Test {};

namespace t = ::takatori::type;

TEST_F(aggregate_configurable_provider_test, add) {
    auto p1 = std::make_shared<configurable_provider>();
    p1->add({
            1,
            "f",
            t::int4(),
            {
                    t::int8(),
            },
    });

    std::vector<declaration const*> r;
    p1->each([&](auto d){
        r.emplace_back(d.get());
    });

    ASSERT_EQ(r.size(), 1);
    EXPECT_EQ(r[0]->definition_id(), 1);
}

TEST_F(aggregate_configurable_provider_test, remove) {
    auto p1 = std::make_shared<configurable_provider>();
    auto& f1 = p1->add({
            1,
            "f",
            t::int4(),
            {
                    t::int4(),
            },
    });
    auto& f2 = p1->add({
            2,
            "f",
            t::int8(),
            {
                    t::int8(),
            },
    });

    EXPECT_TRUE(p1->remove(*f1));

    std::vector<declaration const*> r;
    p1->each([&](auto d){
        r.emplace_back(d.get());
    });

    ASSERT_EQ(r.size(), 1);
    EXPECT_EQ(r[0], f2.get());
}

TEST_F(aggregate_configurable_provider_test, each_name) {
    auto p1 = std::make_shared<configurable_provider>();
    p1->add({
            1,
            "f1",
            t::int4(),
            {
                    t::int8(),
            },
    });
    p1->add({
            2,
            "f2",
            t::int4(),
            {
                    t::int4(),
            },
    });
    p1->add({
            3,
            "f2",
            t::int8(),
            {
                    t::int8(),
            },
    });
    p1->add({
            4,
            "f2",
            t::int8(),
            {
                    t::int8(),
                    t::int8(),
            },
    });

    std::vector<declaration const*> r;
    p1->each("f2", 1, [&](auto d){
        r.emplace_back(d.get());
    });

    ASSERT_EQ(r.size(), 2);
    std::sort(r.begin(), r.end(), [](auto& a, auto& b){
        return a->definition_id() < b->definition_id();
    });
    EXPECT_EQ(r[0]->definition_id(), 2);
    EXPECT_EQ(r[1]->definition_id(), 3);
}

TEST_F(aggregate_configurable_provider_test, each_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto p2 = std::make_shared<configurable_provider>(p1);

    p1->add({
            1,
            "f",
            t::int4(),
            {
                    t::int4(),
            },
    });

    p2->add({
            2,
            "f",
            t::int8(),
            {
                    t::int8(),
            },
    });

    std::vector<declaration const*> r;
    p2->each([&](auto d){
        r.emplace_back(d.get());
    });

    ASSERT_EQ(r.size(), 2);
    std::sort(r.begin(), r.end(), [](auto& a, auto& b){
        return a->definition_id() < b->definition_id();
    });
    EXPECT_EQ(r[0]->definition_id(), 1);
    EXPECT_EQ(r[1]->definition_id(), 2);
}

TEST_F(aggregate_configurable_provider_test, each_name_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto p2 = std::make_shared<configurable_provider>(p1);

    p1->add({
            1,
            "f1",
            t::int4(),
            {
                    t::int8(),
            },
    });
    p1->add({
            2,
            "f2",
            t::int4(),
            {
                    t::int4(),
            },
    });

    p2->add({
            3,
            "f2",
            t::int8(),
            {
                    t::int8(),
            },
    });
    p2->add({
            4,
            "f2",
            t::int8(),
            {
                    t::int8(),
                    t::int8(),
            },
    });

    std::vector<declaration const*> r;
    p2->each("f2", 1, [&](auto d){
        r.emplace_back(d.get());
    });

    ASSERT_EQ(r.size(), 2);
    std::sort(r.begin(), r.end(), [](auto& a, auto& b){
        return a->definition_id() < b->definition_id();
    });
    EXPECT_EQ(r[0]->definition_id(), 2);
    EXPECT_EQ(r[1]->definition_id(), 3);
}

} // namespace yugawara::aggregate
