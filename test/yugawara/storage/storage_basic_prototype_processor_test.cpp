#include <yugawara/storage/basic_prototype_processor.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/value/primitive.h>
#include <takatori/util/clonable.h>

#include <yugawara/storage/table.h>
#include <yugawara/storage/configurable_provider.h>

namespace yugawara::storage {

namespace t = ::takatori::type;
namespace v = ::takatori::value;

using ::takatori::util::clone_shared;

class storage_basic_prototype_processor_test : public ::testing::Test {
protected:
    schema::declaration location { "public" };
    std::shared_ptr<table> tbl = clone_shared(table {
            { 1 },
            "T",
            {
                    {
                            "C0",
                            t::int4 {},
                            {},
                            {},
                            {},
                            "COLUMN-0",
                    },
                    {
                            "C1",
                            t::int4 {},
                            variable::nullable,
                            {},
                            {},
                            "COLUMN-1",
                    },
                    {
                            "C2",
                            t::int8 {},
                            ~variable::nullable,
                            v::int8 { 10 },
                            {},
                            "COLUMN-2",
                    },
            },
            "TABLE",
    });
    index idx {
            2,
            tbl,
            "IDX",
            {
                    {tbl->columns()[0], sort_direction::ascendant,},
                    {tbl->columns()[1], sort_direction::descendant,},
            },
            {
                    tbl->columns()[2],
            },
            {},
            "INDEX",
    };
    std::vector<prototype_processor::diagnostic_type> diagnostics;

    void expect_column_eq(column const& a, column const& b) noexcept {
        EXPECT_NE(std::addressof(a), std::addressof(b));
        EXPECT_NE(std::addressof(a.owner()), std::addressof(b.owner()));
        EXPECT_EQ(a.simple_name(), b.simple_name());
        EXPECT_EQ(a.type(), b.type());
        EXPECT_EQ(a.criteria(), b.criteria());
        EXPECT_EQ(a.default_value(), b.default_value());
        EXPECT_EQ(a.features(), b.features());
        EXPECT_EQ(a.description(), b.description());
    }
};

TEST_F(storage_basic_prototype_processor_test, primary) {
    basic_prototype_processor proc {};
    auto ri = proc(location, idx, [this](auto const& d) { diagnostics.push_back(d); });
    EXPECT_TRUE(diagnostics.empty());
    EXPECT_NE(ri.get(), std::addressof(idx));

    ASSERT_EQ(ri->keys().size(), 2);
    for (std::size_t i = 0; i < ri->keys().size(); ++i) {
        auto&& k0 = idx.keys()[i];
        auto&& k1 = ri->keys()[i];
        expect_column_eq(k0.column(), k1.column());
        EXPECT_EQ(k0.direction(), k1.direction());
    }

    ASSERT_EQ(ri->values().size(), 1);
    for (std::size_t i = 0; i < ri->values().size(); ++i) {
        expect_column_eq(idx.values()[i], ri->values()[i]);
    }
    EXPECT_EQ(ri->features(), idx.features());
    EXPECT_EQ(ri->description(), idx.description());

    ASSERT_TRUE(ri);
    auto& rt = ri->table();
    EXPECT_NE(std::addressof(rt), tbl.get());
    EXPECT_EQ(rt.definition_id(), tbl->definition_id());
    EXPECT_EQ(rt.simple_name(), tbl->simple_name());
    ASSERT_EQ(rt.columns().size(), tbl->columns().size());
    for (std::size_t i = 0; i < rt.columns().size(); ++i) {
        expect_column_eq(rt.columns()[i], tbl->columns()[i]);
    }
    EXPECT_EQ(rt.description(), tbl->description());
}

TEST_F(storage_basic_prototype_processor_test, secondary) {
    basic_prototype_processor proc {};
    auto ri = proc(location, idx, tbl, [this](auto const& d) { diagnostics.push_back(d); });
    EXPECT_TRUE(diagnostics.empty());
    EXPECT_NE(ri.get(), std::addressof(idx));

    ASSERT_EQ(ri->keys().size(), 2);
    for (std::size_t i = 0; i < ri->keys().size(); ++i) {
        auto&& k0 = idx.keys()[i];
        auto&& k1 = ri->keys()[i];
        EXPECT_EQ(k0.column(), k1.column());
        EXPECT_EQ(k0.direction(), k1.direction());
    }

    ASSERT_EQ(ri->values().size(), 1);
    for (std::size_t i = 0; i < ri->values().size(); ++i) {
        EXPECT_EQ(idx.values()[i], ri->values()[i]);
    }
    EXPECT_EQ(ri->features(), idx.features());
    EXPECT_EQ(ri->description(), idx.description());

    ASSERT_TRUE(ri);
    EXPECT_EQ(std::addressof(ri->table()), tbl.get());
}

TEST_F(storage_basic_prototype_processor_test, primary_failure) {
    class mock : public basic_prototype_processor {
    protected:
        bool ensure(
                schema::declaration const&,
                table& t,
                index& i,
                diagnostic_consumer_type const&) override {
            if (std::addressof(t) != std::addressof(i.table())) {
                throw std::runtime_error("invalid");
            }
            return false;
        }
    };
    mock proc {};
    auto ri = proc(location, idx, [this](auto const& d) { diagnostics.push_back(d); });
    ASSERT_FALSE(ri);
}

TEST_F(storage_basic_prototype_processor_test, secondary_failure) {
    class mock : public basic_prototype_processor {
    protected:
        bool ensure(
                schema::declaration const&,
                index&,
                diagnostic_consumer_type const&) override {
            return false;
        }
    };
    mock proc {};
    auto ri = proc(location, idx, tbl, [this](auto const& d) { diagnostics.push_back(d); });
    ASSERT_FALSE(ri);
}

} // namespace yugawara::storage