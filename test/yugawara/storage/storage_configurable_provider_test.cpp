#include <yugawara/storage/configurable_provider.h>

#include <memory>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/character.h>

namespace yugawara::storage {

namespace t = ::takatori::type;

class storage_configurable_provider_test : public ::testing::Test {
public:
    std::shared_ptr<table const> origin = takatori::util::clone_shared(table {
            "T1",
            {
                    { "C1", t::int4() },
                    { "C2", t::int8() },
                    { "C3", t::character(t::varying) },
            },
    });

    column const& c1 = origin->columns()[0];
    column const& c2 = origin->columns()[1];
    column const& c3 = origin->columns()[2];
};

TEST_F(storage_configurable_provider_test, find_relation) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_table(table {
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_relation("TBL"), element);
    EXPECT_EQ(p1->find_relation("__MISSING__"), nullptr);
}

TEST_F(storage_configurable_provider_test, find_relation_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_table(table {
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    ASSERT_TRUE(element);

    configurable_provider p2 {p1 };
    EXPECT_EQ(p2.find_relation("TBL"), element);
}

TEST_F(storage_configurable_provider_test, each_relation) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& e2 = p1->add_table(table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& e3 = p1->add_table(table {
            "T3",
            {
                    { "C1", t::int4() },
            },
    });

    std::vector<std::shared_ptr<relation const>> saw;
    p1->each_relation([&](auto id, auto& element) {
        if (id != element->simple_name()) {
            throw std::runtime_error("fail");
        }
        saw.emplace_back(element);
    });
    EXPECT_EQ(saw.size(), 3);
    EXPECT_NE(std::find(saw.begin(), saw.end(), e1), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e2), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e3), saw.end());
}

TEST_F(storage_configurable_provider_test, each_relation_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    p1->add_table(table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_table(table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    }, true); // hiding
    auto&& e3 = p2->add_table(table {
            "T3",
            {
                    { "C1", t::int4() },
            },
    });

    std::vector<std::shared_ptr<relation const>> saw;
    p2->each_relation([&](auto id, auto& element) {
        if (id != element->simple_name()) {
            throw std::runtime_error("fail");
        }
        saw.emplace_back(element);
    });
    EXPECT_EQ(saw.size(), 3);
    EXPECT_NE(std::find(saw.begin(), saw.end(), e1), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e2), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e3), saw.end());
}

TEST_F(storage_configurable_provider_test, add_relation_conflict) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });

    EXPECT_THROW(p1->add_table(element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_relation_conflict_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    EXPECT_THROW(p2->add_relation(element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_relation_hide_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    }, true); // hiding

    EXPECT_NE(e1, e2);
    EXPECT_EQ(p2->find_relation("T1"), e2);
}

TEST_F(storage_configurable_provider_test, add_relation_overwrite) {
    auto p1 = std::make_shared<configurable_provider>();
    auto e1 = p1->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto e2 = p1->add_table(table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    }, true);
    ASSERT_TRUE(e1);
    ASSERT_TRUE(e2);
    EXPECT_NE(e1, e2);

    EXPECT_EQ(p1->find_relation("T1"), e2);
}

TEST_F(storage_configurable_provider_test, remove_relation) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_table(table {
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_relation("TBL"), element);

    EXPECT_FALSE(p1->remove_relation("__MISSING__"));
    EXPECT_TRUE(p1->remove_relation("TBL"));
}

TEST_F(storage_configurable_provider_test, add_relation_owner) {
    auto p1 = std::make_shared<configurable_provider>();
    auto element = p1->add_relation(table {
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(element->owner().get(), p1.get());

    p1->remove_relation("TBL");
    EXPECT_EQ(element->owner().get(), nullptr);
}

TEST_F(storage_configurable_provider_test, add_table_owner) {
    auto p1 = std::make_shared<configurable_provider>();
    auto element = p1->add_table({
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(element->owner().get(), p1.get());

    p1->remove_relation("T1");
    EXPECT_EQ(element->owner().get(), nullptr);
}

TEST_F(storage_configurable_provider_test, find_index) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index({
            origin,
            "IDX",
            {
                    c1,
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_index("IDX"), element);
    EXPECT_EQ(p1->find_index("__MISSING__"), nullptr);
}

TEST_F(storage_configurable_provider_test, find_index_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index({
            origin,
            "IDX",
            {
                    c1,
            },
    });
    ASSERT_TRUE(element);

    configurable_provider p2 {p1 };
    EXPECT_EQ(p2.find_index("IDX"), element);
}

TEST_F(storage_configurable_provider_test, each_index) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_index({
            origin,
            "I1",
            {
                    c1,
            },
    });
    auto&& e2 = p1->add_index({
            origin,
            "I2",
            {
                    c1,
            },
    });
    auto&& e3 = p1->add_index({
            origin,
            "I3",
            {
                    c1,
            },
    });

    std::vector<std::shared_ptr<index const>> saw;
    p1->each_index([&](auto id, auto& element) {
        if (id != element->simple_name()) {
            throw std::runtime_error("fail");
        }
        saw.emplace_back(element);
    });
    EXPECT_EQ(saw.size(), 3);
    EXPECT_NE(std::find(saw.begin(), saw.end(), e1), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e2), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e3), saw.end());
}

TEST_F(storage_configurable_provider_test, each_index_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_index({
            origin,
            "I1",
            {
                    c1,
            },
    });
    p1->add_index({
            origin,
            "I2",
            {
                    c1,
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_index({
            origin,
            "I2",
            {
                    c1,
            },
    }, true); // hiding
    auto&& e3 = p2->add_index({
            origin,
            "I3",
            {
                    c1,
            },
    });

    std::vector<std::shared_ptr<index const>> saw;
    p2->each_index([&](auto id, auto& element) {
        if (id != element->simple_name()) {
            throw std::runtime_error("fail");
        }
        saw.emplace_back(element);
    });
    EXPECT_EQ(saw.size(), 3);
    EXPECT_NE(std::find(saw.begin(), saw.end(), e1), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e2), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), e3), saw.end());
}

TEST_F(storage_configurable_provider_test, find_primary_index) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& parent = p1->add_table({
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& element = p1->add_index({
            parent,
            "IDX",
            {
                    parent->columns()[0],
            },
            {},
            {
                index_feature::primary,
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_primary_index(*parent), element);
}

TEST_F(storage_configurable_provider_test, find_primary_index_secondary) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& parent = p1->add_table({
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& element = p1->add_index({
            parent,
            "IDX",
            {
                    parent->columns()[0],
            },
            {},
            {
            },
    });
    ASSERT_TRUE(element);
    EXPECT_FALSE(p1->find_primary_index(*parent));
}

TEST_F(storage_configurable_provider_test, find_primary_index_table_inconsistent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& parent = p1->add_table({
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& element = p1->add_index({
            origin,
            "IDX",
            {
                    parent->columns()[0],
            },
            {},
            {
                    index_feature::primary,
            },
    });
    ASSERT_TRUE(element);
    EXPECT_FALSE(p1->find_primary_index(*parent));
}

TEST_F(storage_configurable_provider_test, find_primary_index_shortcircuit) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& parent = p1->add_table({
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& element = p1->add_index({
            parent,
            parent->simple_name(),
            {
                    parent->columns()[0],
            },
            {},
            {
                    index_feature::primary,
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_primary_index(*parent), element);
}

TEST_F(storage_configurable_provider_test, find_primary_index_shortcircuit_secondary) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& parent = p1->add_table({
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& element = p1->add_index({
            parent,
            parent->simple_name(),
            {
                    parent->columns()[0],
            },
            {},
            {
            },
    });
    ASSERT_TRUE(element);
    EXPECT_FALSE(p1->find_primary_index(*parent));
}

TEST_F(storage_configurable_provider_test, find_primary_index_shortcircuit_table_inconsistent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& parent = p1->add_table({
            "TBL",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& element = p1->add_index({
            origin,
            parent->simple_name(),
            {
                    parent->columns()[0],
            },
            {},
            {
                    index_feature::primary,
            },
    });
    ASSERT_TRUE(element);
    EXPECT_FALSE(p1->find_primary_index(*parent));
}

TEST_F(storage_configurable_provider_test, add_index_conflict) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index({
            origin,
            "I1",
            {
                    c1,
            },
    });

    EXPECT_THROW(p1->add_index(element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_index_conflict_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index({
            origin,
            "I1",
            {
                    c1,
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    EXPECT_THROW(p2->add_index(element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_index_hide_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_index({
            origin,
            "IDX",
            {
                    c1,
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_index({
            origin,
            "IDX",
            {
                    c1,
            },
    }, true); // hiding

    EXPECT_NE(e1, e2);
    EXPECT_EQ(p2->find_index("IDX"), e2);
}

TEST_F(storage_configurable_provider_test, add_index_overwrite) {
    auto p1 = std::make_shared<configurable_provider>();
    auto e1 = p1->add_index({
            origin,
            "IDX",
            {
                    c1,
            }
    });
    auto e2 = p1->add_index({
            origin,
            "IDX",
            {
                    c1,
            }
    }, true);
    ASSERT_TRUE(e1);
    ASSERT_TRUE(e2);
    EXPECT_NE(e1, e2);

    EXPECT_EQ(p1->find_index("IDX"), e2);
}

TEST_F(storage_configurable_provider_test, remove_index) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index({
            origin,
            "IDX",
            {
                    c1,
            }
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_index("IDX"), element);

    EXPECT_FALSE(p1->remove_index("__MISSING__"));
    EXPECT_TRUE(p1->remove_index("IDX"));
}

TEST_F(storage_configurable_provider_test, add_sequence) {
    auto p1 = std::make_shared<configurable_provider>();
    auto element = p1->add_sequence(sequence { "S1" });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_sequence("S1"), element);
    EXPECT_EQ(p1->find_relation("__MISSING__"), nullptr);
    
    EXPECT_EQ(element->owner().get(), p1.get());
}

TEST_F(storage_configurable_provider_test, remove_sequence) {
    auto p1 = std::make_shared<configurable_provider>();
    auto s1 = p1->add_sequence(sequence { "S1" });
    auto s2 = p1->add_sequence(sequence { "S2" });
    auto s3 = p1->add_sequence(sequence { "S3" });

    EXPECT_EQ(s2->owner().get(), p1.get());
    p1->remove_sequence("S2");
    EXPECT_EQ(s2->owner().get(), nullptr);

    std::vector<std::shared_ptr<sequence const>> saw;
    p1->each_sequence([&](auto id, auto& element) {
        if (id != element->simple_name()) {
            throw std::runtime_error("fail");
        }
        saw.emplace_back(element);
    });
    EXPECT_EQ(saw.size(), 2);
    EXPECT_NE(std::find(saw.begin(), saw.end(), s1), saw.end());
    EXPECT_EQ(std::find(saw.begin(), saw.end(), s2), saw.end());
    EXPECT_NE(std::find(saw.begin(), saw.end(), s3), saw.end());
}

} // namespace yugawara::storage
