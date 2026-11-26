#include <yugawara/storage/partition/prefix_extractor.h>

#include <gtest/gtest.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>
#include <takatori/type/character.h>
#include <takatori/type/octet.h>
#include <takatori/type/date.h>
#include <takatori/type/time_of_day.h>
#include <takatori/type/time_point.h>

#include <takatori/value/primitive.h>
#include <takatori/value/decimal.h>
#include <takatori/value/character.h>
#include <takatori/value/octet.h>
#include <takatori/value/date.h>
#include <takatori/value/time_point.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

namespace yugawara::storage::partition {

namespace t = ::takatori::type;
namespace v = ::takatori::value;

class prefix_extractor_test : public ::testing::Test {
public:
    static v::decimal dec(std::int64_t coefficient, std::int32_t exponent) {
        return v::decimal { ::takatori::decimal::triple { coefficient, exponent } };
    }

    static v::time_point ts(
            std::uint32_t hour,
            std::uint32_t minute,
            std::uint32_t second,
            std::uint64_t nanos = 0) {
        return v::time_point { 2000, 1, 1, hour, minute, second, ::takatori::datetime::time_of_day::time_unit { nanos } };
    }

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
    EXPECT_TRUE(is_applicable(k::floor_fractional, t::decimal { 10, 2 }));
    EXPECT_TRUE(is_applicable(k::floor_fractional, t::decimal {}));

    // integral types are not applicable, because the resulting type must not be larger than the source type
    EXPECT_FALSE(is_applicable(k::floor_fractional, t::int1 {}));
    EXPECT_FALSE(is_applicable(k::floor_fractional, t::int4 {}));
    EXPECT_FALSE(is_applicable(k::floor_fractional, t::int8 {}));
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
    EXPECT_EQ(*compute_type(floor_fractional { 1 }, t::decimal { 10, 2 }), t::decimal {});
    EXPECT_EQ(*compute_type(floor_fractional { 0 }, t::decimal {}), t::decimal {});
    EXPECT_TRUE(is_error(compute_type(floor_fractional { 1 }, t::int4 {})));
    EXPECT_TRUE(is_error(compute_type(floor_fractional { 1 }, t::int8 {})));
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

TEST_F(prefix_extractor_test, is_valid_output_character_prefix) {
    character_prefix e { 3 };
    EXPECT_TRUE(is_valid_output(e, v::character { "" }));
    EXPECT_TRUE(is_valid_output(e, v::character { "abc" }));
    EXPECT_FALSE(is_valid_output(e, v::character { "abcd" }));
    // counts in characters, not in octets
    EXPECT_TRUE(is_valid_output(e, v::character { "\u3042\u3044\u3046" }));
    EXPECT_FALSE(is_valid_output(e, v::character { "\u3042\u3044\u3046\u3048" }));
    EXPECT_FALSE(is_valid_output(e, v::octet { "abc" }));
    EXPECT_TRUE(is_valid_output(e, v::unknown {}));
}

TEST_F(prefix_extractor_test, is_valid_output_binary_prefix) {
    binary_prefix e { 3 };
    EXPECT_TRUE(is_valid_output(e, v::octet { "abc" }));
    EXPECT_FALSE(is_valid_output(e, v::octet { "abcd" }));
    EXPECT_FALSE(is_valid_output(e, v::character { "abc" }));
    EXPECT_TRUE(is_valid_output(e, v::unknown {}));
}

TEST_F(prefix_extractor_test, is_valid_output_floor_division) {
    floor_division e { 10 };
    EXPECT_TRUE(is_valid_output(e, v::int4 { 15 }));
    EXPECT_TRUE(is_valid_output(e, v::int8 { -15 }));
    EXPECT_TRUE(is_valid_output(e, dec(15, 0)));
    EXPECT_TRUE(is_valid_output(e, dec(1500, -2)));
    EXPECT_FALSE(is_valid_output(e, dec(1501, -2)));
    EXPECT_FALSE(is_valid_output(e, v::float8 { 15.0 }));
    EXPECT_FALSE(is_valid_output(e, v::float4 { 15.0F }));
    EXPECT_TRUE(is_valid_output(e, v::unknown {}));
}

TEST_F(prefix_extractor_test, is_valid_output_floor_fractional) {
    floor_fractional e { 2 };
    EXPECT_TRUE(is_valid_output(e, v::int4 { 1 }));
    EXPECT_TRUE(is_valid_output(e, dec(125, -2)));
    EXPECT_TRUE(is_valid_output(e, dec(12500, -4)));
    EXPECT_FALSE(is_valid_output(e, dec(1251, -3)));
    EXPECT_TRUE(is_valid_output(e, dec(-125, -2)));
    EXPECT_TRUE(is_valid_output(e, dec(0, -10)));
    EXPECT_FALSE(is_valid_output(e, v::float8 { 1.25 }));
    EXPECT_FALSE(is_valid_output(floor_fractional { 0 }, dec(15, -1)));
}

TEST_F(prefix_extractor_test, is_valid_output_floor_fractional_large_coefficient) {
    // 10^37 = 0x0785'ee10'd5da'46d9'00f4'36a0'0000'0000
    ::takatori::decimal::triple exact { +1, 0x0785'ee10'd5da'46d9ULL, 0x00f4'36a0'0000'0000ULL, -37 };
    EXPECT_TRUE(is_valid_output(floor_fractional { 0 }, v::decimal { exact }));
    // 10^37 + 1
    ::takatori::decimal::triple large { +1, 0x0785'ee10'd5da'46d9ULL, 0x00f4'36a0'0000'0001ULL, -37 };
    EXPECT_FALSE(is_valid_output(floor_fractional { 0 }, v::decimal { large }));
    EXPECT_FALSE(is_valid_output(floor_fractional { 36 }, v::decimal { large }));
    EXPECT_TRUE(is_valid_output(floor_fractional { 37 }, v::decimal { large }));
}

TEST_F(prefix_extractor_test, is_valid_output_date_truncate) {
    EXPECT_TRUE(is_valid_output(date_truncate { date_unit::year }, v::int4 { 2000 }));
    EXPECT_TRUE(is_valid_output(date_truncate { date_unit::year }, dec(20000, -1)));
    EXPECT_FALSE(is_valid_output(date_truncate { date_unit::year }, dec(20005, -1)));
    EXPECT_FALSE(is_valid_output(date_truncate { date_unit::year }, v::date { 2000, 1, 1 }));
    EXPECT_TRUE(is_valid_output(date_truncate { date_unit::month }, v::date { 2000, 1, 1 }));
    EXPECT_FALSE(is_valid_output(date_truncate { date_unit::month }, v::date { 2000, 1, 15 }));
    EXPECT_TRUE(is_valid_output(date_truncate { date_unit::day }, v::date { 2000, 1, 15 }));
    EXPECT_FALSE(is_valid_output(date_truncate { date_unit::day }, ts(0, 0, 0)));
    EXPECT_TRUE(is_valid_output(date_truncate { date_unit::month }, v::unknown {}));
}

TEST_F(prefix_extractor_test, is_valid_output_timestamp_truncate) {
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::year }, v::int8 { 2000 }));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::year }, v::float8 { 2000 }));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::month }, v::date { 2000, 2, 1 }));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::month }, v::date { 2000, 2, 2 }));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::month }, ts(0, 0, 0)));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::day }, v::date { 2000, 2, 2 }));

    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, ts(10, 0, 0)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, ts(10, 1, 0)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, ts(10, 0, 1)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, ts(10, 0, 0, 1)));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, v::date { 2000, 2, 2 }));

    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::minute }, ts(10, 30, 0)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::minute }, ts(10, 30, 1)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::minute }, ts(10, 30, 0, 1)));

    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::second, 0 }, ts(10, 30, 15)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::second, 0 }, ts(10, 30, 15, 100'000'000)));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::second, 3 }, ts(10, 30, 15, 123'000'000)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::second, 3 }, ts(10, 30, 15, 123'400'000)));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::second, 9 }, ts(10, 30, 15, 123'456'789)));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::second, 12 }, ts(10, 30, 15, 123'456'789)));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::second, 3 }, v::unknown {}));
}

TEST_F(prefix_extractor_test, is_valid_output_dispatch) {
    EXPECT_TRUE(is_valid_output(prefix_extractor {}, v::float8 { 1.5 }));
    EXPECT_TRUE(is_valid_output(prefix_extractor { character_prefix { 2 } }, v::character { "ab" }));
    EXPECT_FALSE(is_valid_output(prefix_extractor { character_prefix { 2 } }, v::character { "abc" }));
    EXPECT_FALSE(is_valid_output(prefix_extractor { binary_prefix { 2 } }, v::octet { "abc" }));
    EXPECT_FALSE(is_valid_output(prefix_extractor { floor_division { 10 } }, dec(15, -1)));
    EXPECT_FALSE(is_valid_output(prefix_extractor { floor_fractional { 1 } }, dec(125, -2)));
    EXPECT_FALSE(is_valid_output(prefix_extractor { date_truncate { date_unit::month } }, v::date { 2000, 1, 15 }));
    EXPECT_FALSE(is_valid_output(prefix_extractor { timestamp_truncate { timestamp_unit::hour } }, ts(10, 1, 0)));
}

TEST_F(prefix_extractor_test, is_valid_output_null) {
    EXPECT_TRUE(is_valid_output(floor_fractional { 2 }, v::unknown {}));
    EXPECT_TRUE(is_valid_output(date_truncate { date_unit::year }, v::unknown {}));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, v::unknown {}));
    EXPECT_TRUE(is_valid_output(prefix_extractor {}, v::unknown {}));
    EXPECT_TRUE(is_valid_output(prefix_extractor { character_prefix { 2 } }, v::unknown {}));
}

TEST_F(prefix_extractor_test, is_valid_output_approximate_number) {
    EXPECT_FALSE(is_valid_output(character_prefix { 3 }, v::float8 { 1.0 }));
    EXPECT_FALSE(is_valid_output(binary_prefix { 3 }, v::float8 { 1.0 }));
    EXPECT_FALSE(is_valid_output(floor_fractional { 0 }, v::float4 { 1.0F }));
    EXPECT_FALSE(is_valid_output(date_truncate { date_unit::year }, v::float8 { 2000.0 }));
    EXPECT_FALSE(is_valid_output(date_truncate { date_unit::month }, v::float8 { 1.0 }));
    EXPECT_FALSE(is_valid_output(date_truncate { date_unit::day }, v::float8 { 1.0 }));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, v::float8 { 1.0 }));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::second, 3 }, v::float8 { 1.0 }));
}

TEST_F(prefix_extractor_test, is_valid_output_exact_number_forms) {
    // negative values
    EXPECT_TRUE(is_valid_output(floor_division { 10 }, dec(-1500, -2)));
    EXPECT_FALSE(is_valid_output(floor_division { 10 }, dec(-1501, -2)));
    // positive exponents
    EXPECT_TRUE(is_valid_output(floor_division { 10 }, dec(15, 2)));
    EXPECT_TRUE(is_valid_output(floor_fractional { 0 }, dec(15, 2)));
    // 2^64 * 10^-1: the high word is not zero, and the lowest digit is not zero
    ::takatori::decimal::triple high_only { +1, 1ULL, 0ULL, -1 };
    EXPECT_FALSE(is_valid_output(floor_fractional { 0 }, v::decimal { high_only }));
    EXPECT_TRUE(is_valid_output(floor_fractional { 1 }, v::decimal { high_only }));
}

TEST_F(prefix_extractor_test, is_valid_output_binary_prefix_empty) {
    EXPECT_TRUE(is_valid_output(binary_prefix { 3 }, v::octet { "" }));
}

TEST_F(prefix_extractor_test, is_valid_output_timestamp_truncate_types) {
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::year }, dec(20000, -1)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::year }, dec(20005, -1)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::day }, ts(0, 0, 0)));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::hour }, v::int4 { 10 }));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::minute }, v::character { "10:30" }));
    EXPECT_FALSE(is_valid_output(timestamp_truncate { timestamp_unit::second, 3 }, v::int8 { 0 }));
    // DATE values represent the beginning of the day
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::minute }, v::date { 2000, 2, 2 }));
    EXPECT_TRUE(is_valid_output(timestamp_truncate { timestamp_unit::second, 0 }, v::date { 2000, 2, 2 }));
}

} // namespace yugawara::storage::partition
