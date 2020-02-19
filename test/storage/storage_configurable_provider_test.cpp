#include <yugawara/storage/configurable_provider.h>

#include <memory>

#include <gtest/gtest.h>

#include <takatori/type/int.h>
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
    auto&& element = p1->add_relation("TBL", table {
            "T1",
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
    auto&& element = p1->add_relation("TBL", table {
            "T1",
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
    auto&& e1 = p1->add_relation("T1", table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& e2 = p1->add_relation("T2", table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    });
    auto&& e3 = p1->add_relation("T3", table {
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
    auto&& e1 = p1->add_relation("T1", table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    p1->add_relation("T2", table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_relation("T2", table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    }, true); // hiding
    auto&& e3 = p2->add_relation("T3", table {
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
    auto&& element = p1->add_relation("TBL", table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });

    EXPECT_THROW(p1->add_relation("TBL", element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_relation_conflict_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_relation("TBL", table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    EXPECT_THROW(p2->add_relation("TBL", element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_relation_hide_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_relation("TBL", table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_relation("TBL", table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    }, true); // hiding

    EXPECT_NE(e1, e2);
    EXPECT_EQ(p2->find_relation("TBL"), e2);
}

TEST_F(storage_configurable_provider_test, add_relation_overwrite) {
    auto p1 = std::make_shared<configurable_provider>();
    auto e1 = p1->add_relation("TBL", table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    auto e2 = p1->add_relation("TBL", table {
            "T2",
            {
                    { "C1", t::int4() },
            },
    }, true);
    ASSERT_TRUE(e1);
    ASSERT_TRUE(e2);
    EXPECT_NE(e1, e2);

    EXPECT_EQ(p1->find_relation("TBL"), e2);
}

TEST_F(storage_configurable_provider_test, remove_relation) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_relation("TBL", table {
            "T1",
            {
                    { "C1", t::int4() },
            },
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_relation("TBL"), element);

    EXPECT_FALSE(p1->remove_relation("__MISSING__"));
    EXPECT_TRUE(p1->remove_relation("TBL"));
}

TEST_F(storage_configurable_provider_test, find_index) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index("IDX", {
            origin,
            "I1",
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
    auto&& element = p1->add_index("IDX", {
            origin,
            "I1",
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
    auto&& e1 = p1->add_index("I1", {
            origin,
            "I1",
            {
                    c1,
            },
    });
    auto&& e2 = p1->add_index("I2", {
            origin,
            "I2",
            {
                    c1,
            },
    });
    auto&& e3 = p1->add_index("I3", {
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
    auto&& e1 = p1->add_index("I1", {
            origin,
            "I1",
            {
                    c1,
            },
    });
    p1->add_index("I2", {
            origin,
            "I2",
            {
                    c1,
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_index("I2", {
            origin,
            "I2",
            {
                    c1,
            },
    }, true); // hiding
    auto&& e3 = p2->add_index("I3", {
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

TEST_F(storage_configurable_provider_test, add_index_conflict) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index("IDX", {
            origin,
            "I1",
            {
                    c1,
            },
    });

    EXPECT_THROW(p1->add_index("IDX", element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_index_conflict_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& element = p1->add_index("IDX", {
            origin,
            "I1",
            {
                    c1,
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    EXPECT_THROW(p2->add_index("IDX", element), std::invalid_argument);
}

TEST_F(storage_configurable_provider_test, add_index_hide_in_parent) {
    auto p1 = std::make_shared<configurable_provider>();
    auto&& e1 = p1->add_index("IDX", {
            origin,
            "I1",
            {
                    c1,
            },
    });

    auto p2 = std::make_shared<configurable_provider>(p1);
    auto&& e2 = p2->add_index("IDX", {
            origin,
            "I1",
            {
                    c1,
            },
    }, true); // hiding

    EXPECT_NE(e1, e2);
    EXPECT_EQ(p2->find_index("IDX"), e2);
}

TEST_F(storage_configurable_provider_test, add_index_overwrite) {
    auto p1 = std::make_shared<configurable_provider>();
    auto e1 = p1->add_index("IDX", {
            origin,
            "I1",
            {
                    c1,
            }
    });
    auto e2 = p1->add_index("IDX", {
            origin,
            "I1",
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
    auto&& element = p1->add_index("IDX", {
            origin,
            "I1",
            {
                    c1,
            }
    });
    ASSERT_TRUE(element);
    EXPECT_EQ(p1->find_index("IDX"), element);

    EXPECT_FALSE(p1->remove_index("__MISSING__"));
    EXPECT_TRUE(p1->remove_index("IDX"));
}

} // namespace yugawara::storage
