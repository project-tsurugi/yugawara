#include <yugawara/storage/partition/prefix_extractor.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

namespace yugawara::storage::partition {

namespace t = ::takatori::type;

class prefix_extractor_test : public ::testing::Test {
public:
    static bool is_error(std::shared_ptr<t::data const> const& type) {
        return type && type->kind() == t::extension::tag
            && extension::type::error::is_instance(static_cast<t::extension const&>(*type)); // NOLINT
    }

    static bool is_pending(std::shared_ptr<t::data const> const& type) {
        return type && type->kind() == t::extension::tag
            && extension::type::pending::is_instance(static_cast<t::extension const&>(*type)); // NOLINT
    }
};

TEST_F(prefix_extractor_test, nothing) {
    prefix_extractor e {};
    EXPECT_EQ(e.kind(), prefix_extractor_kind::nothing);
    EXPECT_FALSE(e);
}

TEST_F(prefix_extractor_test, character_prefix) {
    prefix_extractor e { character_prefix { 3 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::character_prefix);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::character_prefix>(), character_prefix { 3 });
}

TEST_F(prefix_extractor_test, binary_prefix) {
    prefix_extractor e { binary_prefix { 4 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::binary_prefix);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::binary_prefix>(), binary_prefix { 4 });
}

TEST_F(prefix_extractor_test, floor_division) {
    prefix_extractor e { floor_division { 1000 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::floor_division);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::floor_division>(), floor_division { 1000 });
}

TEST_F(prefix_extractor_test, floor_fractional) {
    prefix_extractor e { floor_fractional { 2 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::floor_fractional);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::floor_fractional>(), floor_fractional { 2 });
}

TEST_F(prefix_extractor_test, date_truncate) {
    prefix_extractor e { date_truncate { date_unit::month } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::date_truncate);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::date_truncate>(), date_truncate { date_unit::month });
}

TEST_F(prefix_extractor_test, timestamp_truncate) {
    prefix_extractor e { timestamp_truncate { timestamp_unit::second, 3 } };
    EXPECT_EQ(e.kind(), prefix_extractor_kind::timestamp_truncate);
    EXPECT_TRUE(e);
    EXPECT_EQ(e.element<prefix_extractor_kind::timestamp_truncate>(), (timestamp_truncate { timestamp_unit::second, 3 }));
}

TEST_F(prefix_extractor_test, equals) {
    EXPECT_EQ(prefix_extractor {}, prefix_extractor {});
    EXPECT_EQ(prefix_extractor { character_prefix { 3 } }, prefix_extractor { character_prefix { 3 } });
    EXPECT_NE(prefix_extractor { character_prefix { 3 } }, prefix_extractor { character_prefix { 4 } });
    EXPECT_NE(prefix_extractor { character_prefix { 3 } }, prefix_extractor { binary_prefix { 3 } });
    EXPECT_NE(prefix_extractor { character_prefix { 3 } }, prefix_extractor {});
}

TEST_F(prefix_extractor_test, output) {
    std::cout << prefix_extractor {} << std::endl;
    std::cout << prefix_extractor { character_prefix { 3 } } << std::endl;
    std::cout << prefix_extractor { binary_prefix { 4 } } << std::endl;
    std::cout << prefix_extractor { floor_division { 1000 } } << std::endl;
    std::cout << prefix_extractor { floor_fractional { 2 } } << std::endl;
    std::cout << prefix_extractor { date_truncate { date_unit::month } } << std::endl;
    std::cout << prefix_extractor { timestamp_truncate { timestamp_unit::second, 3 } } << std::endl;
}

TEST_F(prefix_extractor_test, is_applicable_nothing) {
    using k = prefix_extractor_kind;
    EXPECT_TRUE(is_applicable(k::nothing, t::int4 {}));
    EXPECT_TRUE(is_applicable(k::nothing, t::float8 {}));
    EXPECT_TRUE(is_applicable(k::nothing, t::boolean {}));
    EXPECT_TRUE(is_applicable(k::nothing, t::time_point { t::with_time_zone }));
}

TEST_F(prefix_extractor_test, is_applicable_character_prefix) {
    using k = prefix_extractor_kind;
    EXPECT_TRUE(is_applicable(k::character_prefix, t::character { 4 }));
    EXPECT_TRUE(is_applicable(k::character_prefix, t::character { t::varying, 10 }));
    EXPECT_TRUE(is_applicable(k::character_prefix, t::character { t::varying }));

    EXPECT_FALSE(is_applicable(k::character_prefix, t::octet { t::varying, 10 }));
    EXPECT_FALSE(is_applicable(k::character_prefix, t::int4 {}));
}

TEST_F(prefix_extractor_test, is_applicable_binary_prefix) {
    using k = prefix_extractor_kind;
    EXPECT_TRUE(is_applicable(k::binary_prefix, t::octet { 4 }));
    EXPECT_TRUE(is_applicable(k::binary_prefix, t::octet { t::varying, 10 }));
    EXPECT_TRUE(is_applicable(k::binary_prefix, t::octet { t::varying }));

    EXPECT_FALSE(is_applicable(k::binary_prefix, t::character { t::varying, 10 }));
    EXPECT_FALSE(is_applicable(k::binary_prefix, t::int4 {}));
}

TEST_F(prefix_extractor_test, is_applicable_floor_division) {
    using k = prefix_extractor_kind;
    EXPECT_TRUE(is_applicable(k::floor_division, t::int1 {}));
    EXPECT_TRUE(is_applicable(k::floor_division, t::int2 {}));
    EXPECT_TRUE(is_applicable(k::floor_division, t::int4 {}));
    EXPECT_TRUE(is_applicable(k::floor_division, t::int8 {}));
    EXPECT_TRUE(is_applicable(k::floor_division, t::decimal { 10, 2 }));
    EXPECT_TRUE(is_applicable(k::floor_division, t::decimal {}));

    EXPECT_FALSE(is_applicable(k::floor_division, t::float4 {}));
    EXPECT_FALSE(is_applicable(k::floor_division, t::float8 {}));
    EXPECT_FALSE(is_applicable(k::floor_division, t::character { t::varying, 10 }));
}

TEST_F(prefix_extractor_test, is_applicable_floor_fractional) {
    using k = prefix_extractor_kind;
    EXPECT_TRUE(is_applicable(k::floor_fractional, t::int4 {}));
    EXPECT_TRUE(is_applicable(k::floor_fractional, t::int8 {}));
    EXPECT_TRUE(is_applicable(k::floor_fractional, t::decimal { 10, 2 }));
    EXPECT_TRUE(is_applicable(k::floor_fractional, t::decimal {}));

    EXPECT_FALSE(is_applicable(k::floor_fractional, t::float8 {}));
    EXPECT_FALSE(is_applicable(k::floor_fractional, t::date {}));
}

TEST_F(prefix_extractor_test, is_applicable_date_truncate) {
    using k = prefix_extractor_kind;
    EXPECT_TRUE(is_applicable(k::date_truncate, t::date {}));

    EXPECT_FALSE(is_applicable(k::date_truncate, t::time_point {}));
    EXPECT_FALSE(is_applicable(k::date_truncate, t::time_of_day {}));
    EXPECT_FALSE(is_applicable(k::date_truncate, t::int4 {}));
}

TEST_F(prefix_extractor_test, is_applicable_timestamp_truncate) {
    using k = prefix_extractor_kind;
    EXPECT_TRUE(is_applicable(k::timestamp_truncate, t::time_point {}));

    EXPECT_FALSE(is_applicable(k::timestamp_truncate, t::time_point { t::with_time_zone }));
    EXPECT_FALSE(is_applicable(k::timestamp_truncate, t::date {}));
    EXPECT_FALSE(is_applicable(k::timestamp_truncate, t::time_of_day {}));
}

TEST_F(prefix_extractor_test, is_applicable_erroneous) {
    EXPECT_FALSE(is_applicable(prefix_extractor_kind::floor_division, extension::type::error {}));
}

TEST_F(prefix_extractor_test, is_applicable_element) {
    using k = prefix_extractor_kind;
    using ::takatori::util::enum_tag;
    EXPECT_TRUE(is_applicable(enum_tag<k::character_prefix>, t::character { 4 }));
    EXPECT_FALSE(is_applicable(enum_tag<k::character_prefix>, t::octet { 4 }));
    EXPECT_TRUE(is_applicable(enum_tag<k::binary_prefix>, t::octet { 4 }));
    EXPECT_FALSE(is_applicable(enum_tag<k::binary_prefix>, t::character { 4 }));
    EXPECT_TRUE(is_applicable(enum_tag<k::floor_division>, t::int8 {}));
    EXPECT_FALSE(is_applicable(enum_tag<k::floor_division>, t::float8 {}));
    EXPECT_TRUE(is_applicable(enum_tag<k::floor_fractional>, t::decimal { 10, 2 }));
    EXPECT_FALSE(is_applicable(enum_tag<k::floor_fractional>, t::float8 {}));
    EXPECT_TRUE(is_applicable(enum_tag<k::date_truncate>, t::date {}));
    EXPECT_FALSE(is_applicable(enum_tag<k::date_truncate>, t::time_point {}));
    EXPECT_TRUE(is_applicable(enum_tag<k::timestamp_truncate>, t::time_point {}));
    EXPECT_FALSE(is_applicable(enum_tag<k::timestamp_truncate>, t::time_point { t::with_time_zone }));
    EXPECT_FALSE(is_applicable(enum_tag<k::floor_division>, extension::type::error {}));
}

TEST_F(prefix_extractor_test, compute_type_nothing) {
    EXPECT_EQ(*compute_type({}, t::int4 {}), t::int4 {});
    EXPECT_EQ(*compute_type({}, t::character { 4 }), t::character { 4 });
    EXPECT_EQ(*compute_type({}, t::time_point { t::with_time_zone }), t::time_point { t::with_time_zone });
}

TEST_F(prefix_extractor_test, compute_type_character_prefix) {
    EXPECT_EQ(*compute_type(character_prefix { 2 }, t::character { 4 }), t::character { t::varying });
    EXPECT_EQ(*compute_type(character_prefix { 2 }, t::character { t::varying, 10 }), t::character { t::varying });
}

TEST_F(prefix_extractor_test, compute_type_binary_prefix) {
    EXPECT_EQ(*compute_type(binary_prefix { 2 }, t::octet { 4 }), t::octet { t::varying });
    EXPECT_EQ(*compute_type(binary_prefix { 2 }, t::octet { t::varying, 10 }), t::octet { t::varying });
}

TEST_F(prefix_extractor_test, compute_type_floor_division) {
    EXPECT_EQ(*compute_type(floor_division { 10 }, t::int1 {}), t::int4 {});
    EXPECT_EQ(*compute_type(floor_division { 10 }, t::int2 {}), t::int4 {});
    EXPECT_EQ(*compute_type(floor_division { 10 }, t::int4 {}), t::int4 {});
    EXPECT_EQ(*compute_type(floor_division { 10 }, t::int8 {}), t::int8 {});
    EXPECT_EQ(*compute_type(floor_division { 10 }, t::decimal { 10, 2 }), t::decimal {});
}

TEST_F(prefix_extractor_test, compute_type_floor_fractional) {
    EXPECT_EQ(*compute_type(floor_fractional { 1 }, t::int4 {}), t::int4 {});
    EXPECT_EQ(*compute_type(floor_fractional { 1 }, t::int8 {}), t::int8 {});
    EXPECT_EQ(*compute_type(floor_fractional { 1 }, t::decimal { 10, 2 }), t::decimal {});
}

TEST_F(prefix_extractor_test, compute_type_date_truncate) {
    EXPECT_EQ(*compute_type(date_truncate { date_unit::year }, t::date {}), t::int4 {});
    EXPECT_EQ(*compute_type(date_truncate { date_unit::month }, t::date {}), t::date {});
    EXPECT_EQ(*compute_type(date_truncate { date_unit::day }, t::date {}), t::date {});
}

TEST_F(prefix_extractor_test, compute_type_timestamp_truncate) {
    EXPECT_EQ(*compute_type(timestamp_truncate { timestamp_unit::year }, t::time_point {}), t::int4 {});
    EXPECT_EQ(*compute_type(timestamp_truncate { timestamp_unit::month }, t::time_point {}), t::date {});
    EXPECT_EQ(*compute_type(timestamp_truncate { timestamp_unit::day }, t::time_point {}), t::date {});
    EXPECT_EQ(*compute_type(timestamp_truncate { timestamp_unit::hour }, t::time_point {}), t::time_point {});
    EXPECT_EQ(*compute_type(timestamp_truncate { timestamp_unit::minute }, t::time_point {}), t::time_point {});
    EXPECT_EQ(*compute_type(timestamp_truncate { timestamp_unit::second, 3 }, t::time_point {}), t::time_point {});
}

TEST_F(prefix_extractor_test, compute_type_not_applicable) {
    EXPECT_TRUE(is_error(compute_type(floor_division { 10 }, t::float8 {})));
    EXPECT_TRUE(is_error(compute_type(date_truncate { date_unit::year }, t::time_point {})));
    EXPECT_TRUE(is_error(compute_type(timestamp_truncate { timestamp_unit::day }, t::time_point { t::with_time_zone })));
}

TEST_F(prefix_extractor_test, compute_type_erroneous) {
    EXPECT_TRUE(is_pending(compute_type(floor_division { 10 }, extension::type::error {})));
    EXPECT_TRUE(is_pending(compute_type({}, extension::type::pending {})));
}

TEST_F(prefix_extractor_test, compute_type_dispatch) {
    EXPECT_EQ(*compute_type(prefix_extractor { character_prefix { 2 } }, t::character { 4 }), t::character { t::varying });
    EXPECT_EQ(*compute_type(prefix_extractor { binary_prefix { 2 } }, t::octet { 4 }), t::octet { t::varying });
    EXPECT_EQ(*compute_type(prefix_extractor { floor_division { 10 } }, t::int8 {}), t::int8 {});
    EXPECT_EQ(*compute_type(prefix_extractor { floor_fractional { 1 } }, t::decimal { 10, 2 }), t::decimal {});
    EXPECT_EQ(*compute_type(prefix_extractor { date_truncate { date_unit::month } }, t::date {}), t::date {});
    EXPECT_EQ(*compute_type(prefix_extractor { timestamp_truncate { timestamp_unit::hour } }, t::time_point {}), t::time_point {});
    EXPECT_TRUE(is_error(compute_type(prefix_extractor { floor_division { 10 } }, t::float8 {})));
    EXPECT_TRUE(is_pending(compute_type(prefix_extractor { date_truncate { date_unit::day } }, extension::type::pending {})));
}

} // namespace yugawara::storage::partition
