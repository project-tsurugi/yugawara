#include <yugawara/storage/configurable_provider.h>

#include <memory>
#include <mutex>
#include <string>
#include <vector>

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

namespace {

std::vector<std::shared_ptr<index const>> collect_table_indices(provider const& p, table const& t) {
    std::vector<std::shared_ptr<index const>> results {};
    p.each_table_index(t, [&](std::string_view id, std::shared_ptr<index const> const& element) {
        if (id != element->simple_name()) {
            throw std::runtime_error("fail");
        }
        results.emplace_back(element);
    });
    return results;
}

} // namespace

TEST_F(storage_configurable_provider_test, each_table_index) {
    auto other = takatori::util::clone_shared(table { "T2", { { "C1", t::int4() } } });
    auto p1 = std::make_shared<configurable_provider>();
    auto e1 = p1->add_index({ origin, "I1", { c1 } });
    auto e2 = p1->add_index({ other, "I2", { other->columns()[0] } });
    auto e3 = p1->add_index({ origin, "I3", { c1 } });

    using v = std::vector<std::shared_ptr<index const>>;
    EXPECT_EQ(collect_table_indices(*p1, *origin), (v { e1, e3 }));
    EXPECT_EQ(collect_table_indices(*p1, *other), (v { e2 }));

    table missing { "MISSING", { { "C1", t::int4() } } };
    EXPECT_EQ(collect_table_indices(*p1, missing), (v {}));
}

TEST_F(storage_configurable_provider_test, each_table_index_parent) {
    auto other = takatori::util::clone_shared(table { "T2", { { "C1", t::int4() } } });
    auto p1 = std::make_shared<configurable_provider>();
    auto e1 = p1->add_index({ origin, "I1", { c1 } });
    p1->add_index({ origin, "I2", { c1 } });
    auto e4 = p1->add_index({ origin, "I4", { c1 } });

    auto p2 = std::make_shared<configurable_provider>(p1);
    // hides parent's I2, with an index of the other table
    auto e2 = p2->add_index({ other, "I2", { other->columns()[0] } }, true);
    auto e3 = p2->add_index({ origin, "I3", { c1 } });

    using v = std::vector<std::shared_ptr<index const>>;
    // child first, and then the parent
    EXPECT_EQ(collect_table_indices(*p2, *origin), (v { e3, e1, e4 }));
    EXPECT_EQ(collect_table_indices(*p2, *other), (v { e2 }));
}

TEST_F(storage_configurable_provider_test, each_table_index_overwrite) {
    auto other = takatori::util::clone_shared(table { "T2", { { "C1", t::int4() } } });
    auto p1 = std::make_shared<configurable_provider>();
    p1->add_index({ origin, "I1", { c1 } });
    auto e2 = p1->add_index({ other, "I1", { other->columns()[0] } }, true);

    using v = std::vector<std::shared_ptr<index const>>;
    EXPECT_EQ(collect_table_indices(*p1, *origin), (v {}));
    EXPECT_EQ(collect_table_indices(*p1, *other), (v { e2 }));
    EXPECT_FALSE(p1->find_primary_index(*origin));
}

TEST_F(storage_configurable_provider_test, each_table_index_remove) {
    auto p1 = std::make_shared<configurable_provider>();
    auto e1 = p1->add_index({ origin, "I1", { c1 }, {}, { index_feature::primary } });
    auto e2 = p1->add_index({ origin, "I2", { c1 } });

    EXPECT_TRUE(p1->remove_index("I1"));

    using v = std::vector<std::shared_ptr<index const>>;
    EXPECT_EQ(collect_table_indices(*p1, *origin), (v { e2 }));
    EXPECT_FALSE(p1->find_primary_index(*origin));

    EXPECT_TRUE(p1->remove_index("I2"));
    EXPECT_EQ(collect_table_indices(*p1, *origin), (v {}));

    // re-add after the table has no indices
    auto e3 = p1->add_index({ origin, "I3", { c1 }, {}, { index_feature::primary } });
    EXPECT_EQ(collect_table_indices(*p1, *origin), (v { e3 }));
    EXPECT_EQ(p1->find_primary_index(*origin), e3);
}

TEST_F(storage_configurable_provider_test, each_table_index_same_as_full_scan) {
    std::vector<std::shared_ptr<table const>> tables {};
    for (std::size_t i = 0; i < 5; ++i) {
        tables.emplace_back(takatori::util::clone_shared(table {
                "T" + std::to_string(i),
                { { "C1", t::int4() } },
        }));
    }
    auto p1 = std::make_shared<configurable_provider>();
    for (std::size_t i = 0; i < 20; ++i) {
        auto&& tbl = tables[(i * 3) % tables.size()];
        p1->add_index({ tbl, "I" + std::to_string(i), { tbl->columns()[0] } });
    }
    auto p2 = std::make_shared<configurable_provider>(p1);
    for (std::size_t i = 10; i < 25; ++i) {
        auto&& tbl = tables[(i * 7) % tables.size()];
        p2->add_index({ tbl, "I" + std::to_string(i), { tbl->columns()[0] } }, true);
    }
    p2->remove_index("I12");
    p2->remove_index("I22");

    for (auto&& tbl : tables) {
        std::vector<std::shared_ptr<index const>> expected {};
        // the default implementation, which scans all indices
        p2->provider::each_table_index(*tbl, [&](std::string_view, std::shared_ptr<index const> const& element) {
            expected.emplace_back(element);
        });
        EXPECT_EQ(collect_table_indices(*p2, *tbl), expected) << tbl->simple_name();
    }
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

TEST_F(storage_configurable_provider_test, find_primary_index_in_parent) {
    auto parent = std::make_shared<configurable_provider>();
    auto primary = parent->add_index({ origin, "PRIMARY_PARENT", { c1 }, {}, { index_feature::primary } });
    configurable_provider child { parent };

    EXPECT_EQ(child.find_primary_index(*origin), primary);
}

TEST_F(storage_configurable_provider_test, find_primary_index_absent_in_parent) {
    auto parent = std::make_shared<configurable_provider>();
    parent->add_index({ origin, "SECONDARY_PARENT", { c1 } });
    configurable_provider child { parent };

    EXPECT_FALSE(child.find_primary_index(*origin));
}

TEST_F(storage_configurable_provider_test, find_primary_index_in_ancestor) {
    auto ancestor = std::make_shared<configurable_provider>();
    auto primary = ancestor->add_index({ origin, "PRIMARY_ANCESTOR", { c1 }, {}, { index_feature::primary } });
    auto parent = std::make_shared<configurable_provider>(ancestor);
    configurable_provider child { parent };

    EXPECT_EQ(child.find_primary_index(*origin), primary);
    table missing { "MISSING", { { "C1", t::int4() } } };
    EXPECT_FALSE(child.find_primary_index(missing));
}

TEST_F(storage_configurable_provider_test, find_primary_index_prefers_local) {
    auto parent = std::make_shared<configurable_provider>();
    parent->add_index({ origin, "PRIMARY_PARENT", { c1 }, {}, { index_feature::primary } });
    configurable_provider child { parent };
    auto primary = child.add_index({ origin, "PRIMARY_CHILD", { c1 }, {}, { index_feature::primary } });

    EXPECT_EQ(child.find_primary_index(*origin), primary);
}

TEST_F(storage_configurable_provider_test, find_primary_index_local_secondary_falls_back_to_parent) {
    auto parent = std::make_shared<configurable_provider>();
    auto primary = parent->add_index({ origin, "PRIMARY_PARENT", { c1 }, {}, { index_feature::primary } });
    configurable_provider child { parent };
    child.add_index({ origin, origin->simple_name(), { c1 } });

    EXPECT_EQ(child.find_primary_index(*origin), primary);
}

TEST_F(storage_configurable_provider_test, find_primary_index_local_other_table_falls_back_to_parent) {
    auto parent = std::make_shared<configurable_provider>();
    auto primary = parent->add_index({ origin, "PRIMARY_PARENT", { c1 }, {}, { index_feature::primary } });
    configurable_provider child { parent };
    auto other = takatori::util::clone_shared(table { "T1", { { "C1", t::int4() } } });
    child.add_index({ other, origin->simple_name(), { other->columns()[0] }, {}, { index_feature::primary } });

    EXPECT_EQ(child.find_primary_index(*origin), primary);
}

TEST_F(storage_configurable_provider_test, find_primary_index_in_parent_with_exclusive_mutex) {
    using exclusive_provider = basic_configurable_provider<std::mutex>;
    auto parent = std::make_shared<exclusive_provider>();
    auto primary = parent->add_index({ origin, "PRIMARY_PARENT", { c1 }, {}, { index_feature::primary } });
    exclusive_provider child { parent };

    EXPECT_EQ(child.find_primary_index(*origin), primary);
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
