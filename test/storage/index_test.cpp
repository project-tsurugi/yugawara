#include <yugawara/storage/index.h>

#include <gtest/gtest.h>

#include <takatori/type/int.h>
#include <takatori/type/character.h>
#include <takatori/util/clonable.h>

namespace yugawara::storage {

namespace t = ::takatori::type;
namespace v = ::takatori::value;

class index_test : public ::testing::Test {
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

TEST_F(index_test, simple) {
    index idx {
            origin,
            "I1",
            {
                    c1,
            }
    };

    EXPECT_EQ(std::addressof(idx.table()), origin.get());
    EXPECT_EQ(idx.simple_name(), "I1");
    ASSERT_EQ(idx.keys().size(), 1);
    {
        auto&& k = idx.keys()[0];
        EXPECT_EQ(std::addressof(k.column()), std::addressof(c1));
        EXPECT_EQ(k.direction(), sort_direction::ascendant);
    }
    ASSERT_EQ(idx.values().size(), 0);
    EXPECT_EQ(idx.features(), index_feature_set({ index_feature::find, index_feature::scan }));
}

TEST_F(index_test, keys) {
    index idx {
            origin,
            "I1",
            {
                    c1,
                    { c2, sort_direction::ascendant },
                    { c3, sort_direction::descendant },
            }
    };

    ASSERT_EQ(idx.keys().size(), 3);
    {
        auto&& k = idx.keys()[0];
        EXPECT_EQ(std::addressof(k.column()), std::addressof(c1));
        EXPECT_EQ(k.direction(), sort_direction::ascendant);
    }
    {
        auto&& k = idx.keys()[1];
        EXPECT_EQ(std::addressof(k.column()), std::addressof(c2));
        EXPECT_EQ(k.direction(), sort_direction::ascendant);
    }
    {
        auto&& k = idx.keys()[2];
        EXPECT_EQ(std::addressof(k.column()), std::addressof(c3));
        EXPECT_EQ(k.direction(), sort_direction::descendant);
    }
}

TEST_F(index_test, values) {
    index idx {
            origin,
            "I1",
            { c1, },
            { c2, c3, },
    };

    ASSERT_EQ(idx.values().size(), 2);
}

TEST_F(index_test, features) {
    index_feature_set features {
            index_feature::primary,
            index_feature::find,
            index_feature::scan,
            index_feature::unique,
    };
    index idx {
            origin,
            "I1",
            { c1, },
            {},
            features,
    };
    EXPECT_EQ(idx.features(), features);
}

TEST_F(index_test, output) {
    index idx {
            origin,
            "I1",
            { c1, },
            { c2, c3, },
            { index_feature::find, index_feature::unique },
    };

    std::cout << idx << std::endl;
}

} // namespace yugawara::storage
