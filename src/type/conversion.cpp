#include <yugawara/type/conversion.h>

#include <optional>

#include <cstdint>

#include <takatori/datetime/time_zone.h>
#include <takatori/type/type_kind.h>
#include <takatori/type/dispatch.h>
#include <takatori/util/downcast.h>

#include <yugawara/type/category.h>
#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

namespace yugawara::type {

namespace model = takatori::type;
using kind = model::type_kind;
using takatori::util::unsafe_downcast;
using yugawara::util::ternary;
using yugawara::util::ternary_of;

constexpr std::size_t decimal_precision_int1 = 3;
constexpr std::size_t decimal_precision_int2 = 5;
constexpr std::size_t decimal_precision_int4 = 10;
constexpr std::size_t decimal_precision_int8 = 19;

bool impl::is_conversion_stop_type(model::data const& type) noexcept {
    if (type.kind() == model::extension::tag) {
        auto&& ext = unsafe_downcast<model::extension>(type);
        return extension::type::error::is_instance(ext)
            || extension::type::pending::is_instance(ext);
    }
    return false;
}

using impl::is_conversion_stop_type;

static constexpr std::uint64_t npair(kind a, kind b) noexcept {
    using utype = std::underlying_type_t<kind>;
    static_assert(sizeof(utype) <= sizeof(std::uint64_t) / 2);
    auto scale = static_cast<std::uint64_t>(std::numeric_limits<utype>::max()) + 1;
    return static_cast<std::uint64_t>(a) * scale + static_cast<std::uint64_t>(b);
}

static std::shared_ptr<extension::type::error> shared_error() {
    static auto result = std::make_shared<extension::type::error>();
    return result;
}

static std::shared_ptr<extension::type::pending> shared_pending() {
    static auto result = std::make_shared<extension::type::pending>();
    return result;
}

std::shared_ptr<model::data const> identity_conversion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    return repo.get(type);
}

std::shared_ptr<model::data const> unary_boolean_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::boolean:
        case kind::unknown:
            return repo.get<model::data>(model::boolean());

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const>
binary_boolean_promotion(model::data const& type, model::data const& with, repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        case npair(kind::boolean, kind::boolean):
        case npair(kind::boolean, kind::unknown):
        case npair(kind::unknown, kind::boolean):
        case npair(kind::unknown, kind::unknown):
            return repo.get(model::boolean {});
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> unary_numeric_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::int1:
        case kind::int2:
        case kind::int4:
        case kind::unknown:
            return repo.get(model::int4 {});
        case kind::int8:
        case kind::float4:
        case kind::float8:
        case kind::decimal:
            return repo.get(type);
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> unary_decimal_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::unknown:
        case kind::int1:
            return repo.get(model::decimal { decimal_precision_int1 });
        case kind::int2:
            return repo.get(model::decimal { decimal_precision_int2 });
        case kind::int4:
            return repo.get(model::decimal { decimal_precision_int4 });
        case kind::int8:
            return repo.get(model::decimal { decimal_precision_int8 });
        case kind::decimal:
            return repo.get(type);
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const>
binary_numeric_promotion(model::data const& type, model::data const& with, repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        case npair(kind::int1, kind::int1):
        case npair(kind::int1, kind::int2):
        case npair(kind::int1, kind::int4):
            return repo.get(model::int4 {});
        case npair(kind::int1, kind::int8):
            return repo.get(model::int8 {});
        case npair(kind::int1, kind::decimal):
            return repo.get(model::decimal { decimal_precision_int1 });
        case npair(kind::int1, kind::float4):
            return repo.get(model::float4 {});
        case npair(kind::int1, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::int2, kind::int1):
        case npair(kind::int2, kind::int2):
        case npair(kind::int2, kind::int4):
            return repo.get(model::int4 {});
        case npair(kind::int2, kind::int8):
            return repo.get(model::int8 {});
        case npair(kind::int2, kind::decimal):
            return repo.get(model::decimal { decimal_precision_int2 });
        case npair(kind::int2, kind::float4):
            return repo.get(model::float4 {});
        case npair(kind::int2, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::int4, kind::int1):
        case npair(kind::int4, kind::int2):
        case npair(kind::int4, kind::int4):
            return repo.get(model::int4 {});
        case npair(kind::int4, kind::int8):
            return repo.get(model::int8 {});
        case npair(kind::int4, kind::decimal):
            return repo.get(model::decimal { decimal_precision_int4 });
        case npair(kind::int4, kind::float4):
            return repo.get(model::float8 {});
        case npair(kind::int4, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::int8, kind::int1):
        case npair(kind::int8, kind::int2):
        case npair(kind::int8, kind::int4):
        case npair(kind::int8, kind::int8):
            return repo.get(model::int8 {});
        case npair(kind::int8, kind::decimal):
            return repo.get(model::decimal { decimal_precision_int8 });
        case npair(kind::int8, kind::float4):
            return repo.get(model::float8 {});
        case npair(kind::int8, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::decimal, kind::int1):
        case npair(kind::decimal, kind::int2):
        case npair(kind::decimal, kind::int4):
        case npair(kind::decimal, kind::int8):
        case npair(kind::decimal, kind::decimal):
            return repo.get(type);
        case npair(kind::decimal, kind::float4):
        case npair(kind::decimal, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::float4, kind::int1):
        case npair(kind::float4, kind::int2):
            return repo.get(model::float4 {});
        case npair(kind::float4, kind::int4):
        case npair(kind::float4, kind::int8):
        case npair(kind::float4, kind::decimal):
        case npair(kind::float4, kind::float4):
        case npair(kind::float4, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::float8, kind::int1):
        case npair(kind::float8, kind::int2):
        case npair(kind::float8, kind::int4):
        case npair(kind::float8, kind::int8):
        case npair(kind::float8, kind::decimal):
        case npair(kind::float8, kind::float4):
        case npair(kind::float8, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::int1, kind::unknown):
        case npair(kind::int2, kind::unknown):
        case npair(kind::int4, kind::unknown):
        case npair(kind::int8, kind::unknown):
        case npair(kind::decimal, kind::unknown):
        case npair(kind::float4, kind::unknown):
        case npair(kind::float8, kind::unknown):
            return unary_numeric_promotion(type, repo);

        case npair(kind::unknown, kind::int1):
        case npair(kind::unknown, kind::int2):
        case npair(kind::unknown, kind::int4):
        case npair(kind::unknown, kind::int8):
        case npair(kind::unknown, kind::decimal):
        case npair(kind::unknown, kind::float4):
        case npair(kind::unknown, kind::float8):
            return unary_numeric_promotion(with, repo);

        case npair(kind::unknown, kind::unknown):
            return unary_numeric_promotion(model::unknown {});

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const>
unary_character_string_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::character:
            // FIXME: always varying?
            return repo.get(model::character {
                    model::varying,
                    unsafe_downcast<model::character>(type).length(),
            });
        case kind::unknown:
            // unknown -> zero-length character string
            return repo.get(model::character {
                    model::varying,
                    0,
            });
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> binary_character_string_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        // FIXME: always varying?
        case npair(kind::character, kind::character):
        case npair(kind::character, kind::unknown):
        case npair(kind::unknown, kind::character):
        case npair(kind::unknown, kind::unknown):
            return unary_character_string_promotion(type, repo);

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> unary_bit_string_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::bit:
            // FIXME: always varying?
            return repo.get(model::bit {
                    model::varying,
                    unsafe_downcast<model::bit>(type).length(),
            });
        case kind::unknown:
            // unknown -> zero-length bit string
            return repo.get(model::bit {
                    model::varying,
                    0,
            });
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> binary_bit_string_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        // FIXME: always varying?
        case npair(kind::bit, kind::bit):
        case npair(kind::bit, kind::unknown):
        case npair(kind::unknown, kind::bit):
        case npair(kind::unknown, kind::unknown):
            return unary_bit_string_promotion(type, repo);

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> unary_temporal_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::date:
        case kind::time_of_day:
        case kind::time_point:
            return repo.get(type);
        case kind::unknown:
            return repo.get(model::time_point {});
        default:
            return shared_error();
    }
}

static std::optional<takatori::datetime::time_zone> const&
get_time_zone(model::data const& type) {
    switch (type.kind()) {
        case model::time_of_day::tag:
            return unsafe_downcast<model::time_of_day>(type).time_zone();
        case model::time_point::tag:
            return unsafe_downcast<model::time_point>(type).time_zone();
        default:
            std::abort();
    }
}

static std::optional<takatori::datetime::time_zone>
promote_time_zone(model::data const& a, model::data const& b) {
    auto&& tz1 = get_time_zone(a);
    auto&& tz2 = get_time_zone(b);
    if (tz1 == tz2) {
        return tz1;
    }
    if (!tz1) {
        return tz2;
    }
    if (!tz2) {
        return tz1;
    }
    return takatori::datetime::time_zone::UTC;
}

std::shared_ptr<model::data const> binary_temporal_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        case npair(kind::date, kind::date):
            return unary_temporal_promotion(type, repo);
        case npair(kind::date, kind::time_of_day):
            return repo.get(model::time_point { get_time_zone(with) });
        case npair(kind::date, kind::time_point):
            return unary_temporal_promotion(with, repo);

        case npair(kind::time_of_day, kind::date):
            return repo.get(model::time_point { get_time_zone(type) });
        case npair(kind::time_of_day, kind::time_of_day):
            return repo.get(model::time_of_day { promote_time_zone(type, with) });
        case npair(kind::time_of_day, kind::time_point):
            return repo.get(model::time_point { promote_time_zone(type, with) });

        case npair(kind::time_point, kind::date):
            return unary_temporal_promotion(type, repo);
        case npair(kind::time_point, kind::time_of_day):
        case npair(kind::time_point, kind::time_point):
            return repo.get(model::time_point { promote_time_zone(type, with) });

        case npair(kind::date, kind::unknown):
        case npair(kind::time_of_day, kind::unknown):
        case npair(kind::time_point, kind::unknown):
            return unary_temporal_promotion(type, repo);

        case npair(kind::unknown, kind::date):
        case npair(kind::unknown, kind::time_of_day):
        case npair(kind::unknown, kind::time_point):
        case npair(kind::unknown, kind::unknown):
            return unary_temporal_promotion(with, repo);

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> unary_time_interval_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::datetime_interval:
            return repo.get(type);
        case kind::unknown:
            return repo.get(model::datetime_interval {});
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> binary_time_interval_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        case npair(kind::datetime_interval, kind::datetime_interval):
        case npair(kind::datetime_interval, kind::unknown):
            return repo.get(model::datetime_interval {});

        case npair(kind::unknown, kind::datetime_interval):
        case npair(kind::unknown, kind::unknown):
            return repo.get(model::datetime_interval {});

        default:
            return shared_error();
    }
}

static std::shared_ptr<model::data const> unary_external_promotion(
        model::data const& type,
        repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::extension:
            // FIXME: impl
            return repo.get(type);

        default:
            return shared_error();
    }
}

static std::shared_ptr<model::data const> binary_external_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        case npair(kind::extension, kind::extension):
            // FIXME: impl
            if (type == with) {
                return repo.get(type);
            }
            return shared_error();

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> unifying_conversion(model::data const& type, repository& repo) {
    auto cat = category_of(type);
    switch (cat) {
        case category::boolean:
            return unary_boolean_promotion(type, repo);

        case category::number:
            return unary_numeric_promotion(type, repo);

        case category::character_string:
            return unary_character_string_promotion(type, repo);

        case category::bit_string:
            return unary_bit_string_promotion(type, repo);

        case category::temporal:
            return unary_temporal_promotion(type, repo);

        case category::datetime_interval:
            return unary_time_interval_promotion(type, repo);

        case category::unknown:
        case category::collection:
        case category::structure:
        case category::unique:
            return identity_conversion(type, repo);

        case category::external:
            return unary_external_promotion(type, repo);

        case category::unresolved:
            return shared_pending();
    }
    std::abort();
}

static std::optional<category> unify_category(category a, category b) {
    // unifying conversion is defined only if they are in the same category, except special cases..
    if (a == b) {
        return a;
    }

    // if either category is unresolved, we cannot continue any more conversions
    if (a == category::unresolved || b == category::unresolved) {
        return category::unresolved;
    }

    // if either category is unknown, then becomes the opposite category
    if (a == category::unknown) {
        return b;
    }
    if (b == category::unknown) {
        return a;
    }

    // if either category is external, we continue the conversion with external rules
    if (a == category::external || b == category::external) {
        return category::external;
    }

    // no conversion rules
    return {};
}

std::shared_ptr<model::data const> unifying_conversion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    auto a = category_of(type);
    auto b = category_of(with);
    auto cat = unify_category(a, b);
    if (!cat) {
        return shared_error();
    }
    switch (*cat) {
        case category::unknown:
            return repo.get(type);

        case category::boolean:
            return binary_boolean_promotion(type, with);

        case category::number:
            return binary_numeric_promotion(type, with);

        case category::character_string:
            return binary_character_string_promotion(type, with);

        case category::bit_string:
            return binary_bit_string_promotion(type, with);

        case category::temporal:
            return binary_temporal_promotion(type, with);

        case category::datetime_interval:
            return binary_time_interval_promotion(type, with);

        case category::collection:
        case category::structure:
        case category::unique:
            if (type == with) {
                return identity_conversion(type);
            }
            return shared_error();

        case category::external:
            return binary_external_promotion(type, with, repo);

        case category::unresolved:
            return shared_pending();
    }
    std::abort();
}

ternary is_assignment_convertible(model::data const& type, model::data const& target) noexcept {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(target)) return ternary::unknown;

    switch (npair(type.kind(), target.kind())) {
        case npair(kind::boolean, kind::boolean):

        case npair(kind::int1, kind::int1):
        case npair(kind::int1, kind::int2):
        case npair(kind::int1, kind::int4):
        case npair(kind::int1, kind::int8):
        case npair(kind::int1, kind::decimal):
        case npair(kind::int1, kind::float4):
        case npair(kind::int1, kind::float8):

        case npair(kind::int2, kind::int1):
        case npair(kind::int2, kind::int2):
        case npair(kind::int2, kind::int4):
        case npair(kind::int2, kind::int8):
        case npair(kind::int2, kind::decimal):
        case npair(kind::int2, kind::float4):
        case npair(kind::int2, kind::float8):

        case npair(kind::int4, kind::int1):
        case npair(kind::int4, kind::int2):
        case npair(kind::int4, kind::int4):
        case npair(kind::int4, kind::int8):
        case npair(kind::int4, kind::decimal):
        case npair(kind::int4, kind::float4):
        case npair(kind::int4, kind::float8):

        case npair(kind::int8, kind::int1):
        case npair(kind::int8, kind::int2):
        case npair(kind::int8, kind::int4):
        case npair(kind::int8, kind::int8):
        case npair(kind::int8, kind::decimal):
        case npair(kind::int8, kind::float4):
        case npair(kind::int8, kind::float8):

        case npair(kind::decimal, kind::int1):
        case npair(kind::decimal, kind::int2):
        case npair(kind::decimal, kind::int4):
        case npair(kind::decimal, kind::int8):
        case npair(kind::decimal, kind::decimal):
        case npair(kind::decimal, kind::float4):
        case npair(kind::decimal, kind::float8):

        case npair(kind::float4, kind::int1):
        case npair(kind::float4, kind::int2):
        case npair(kind::float4, kind::int4):
        case npair(kind::float4, kind::int8):
        case npair(kind::float4, kind::decimal):
        case npair(kind::float4, kind::float4):
        case npair(kind::float4, kind::float8):

        case npair(kind::float8, kind::int1):
        case npair(kind::float8, kind::int2):
        case npair(kind::float8, kind::int4):
        case npair(kind::float8, kind::int8):
        case npair(kind::float8, kind::decimal):
        case npair(kind::float8, kind::float4):
        case npair(kind::float8, kind::float8):

        case npair(kind::character, kind::character):

        case npair(kind::bit, kind::bit):

        case npair(kind::date, kind::date):
        case npair(kind::date, kind::time_point):

        case npair(kind::time_of_day, kind::time_of_day):
        case npair(kind::time_of_day, kind::time_point):

        case npair(kind::time_point, kind::date):
        case npair(kind::time_point, kind::time_of_day):
        case npair(kind::time_point, kind::time_point):

        case npair(kind::datetime_interval, kind::datetime_interval):
            return ternary::yes;

        case npair(kind::array, kind::array):
        case npair(kind::record, kind::record):
        case npair(kind::declared, kind::declared):
            return ternary_of(type == target);

        case npair(kind::extension, kind::extension):
            // FIXME: impl assign extension -> extension
            return ternary_of(type == target);

        default:
            return ternary::no;
    }
}

ternary is_cast_convertible(takatori::type::data const& type, takatori::type::data const& target) noexcept {
    auto assignable = is_assignment_convertible(type, target);
    if (assignable != false) {
        return assignable;
    }

    auto src_cat = category_of(type);
    auto dst_cat = category_of(target);

    // always cast convertible from/to character strings
    if (src_cat == category::character_string || dst_cat == category::character_string) {
        return ternary::yes;
    }

    switch (npair(type.kind(), target.kind())) {
        case npair(kind::array, kind::array):
            // FIXME: convertible array of a' -> array of b' only if convertible a' -> b'
            return ternary::no;

        case npair(kind::extension, kind::extension):
            // FIXME: impl cast extension -> extension
            return ternary::no;

        default:
            return ternary::no;
    }
}

util::ternary is_widening_convertible(takatori::type::data const& type, takatori::type::data const& target) noexcept {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(target)) return ternary::unknown;

    switch (npair(type.kind(), target.kind())) {
        case npair(kind::boolean, kind::boolean):

        case npair(kind::int1, kind::int1):
        case npair(kind::int1, kind::int2):
        case npair(kind::int1, kind::int4):
        case npair(kind::int1, kind::int8):
        case npair(kind::int1, kind::float4):
        case npair(kind::int1, kind::float8):

        case npair(kind::int2, kind::int2):
        case npair(kind::int2, kind::int4):
        case npair(kind::int2, kind::int8):
        case npair(kind::int2, kind::float4):
        case npair(kind::int2, kind::float8):

        case npair(kind::int4, kind::int4):
        case npair(kind::int4, kind::int8):
        case npair(kind::int4, kind::float4):
        case npair(kind::int4, kind::float8):

        case npair(kind::int8, kind::int8):
        case npair(kind::int8, kind::float4):
        case npair(kind::int8, kind::float8):

        case npair(kind::decimal, kind::float4):
        case npair(kind::decimal, kind::float8):

        case npair(kind::float4, kind::float4):
        case npair(kind::float4, kind::float8):

        case npair(kind::float8, kind::float8):

        case npair(kind::date, kind::date):
        case npair(kind::date, kind::time_point):

        case npair(kind::time_of_day, kind::time_of_day):
        case npair(kind::time_of_day, kind::time_point):

        case npair(kind::time_point, kind::time_point):

        case npair(kind::datetime_interval, kind::datetime_interval):
            return ternary::yes;

        case npair(kind::int1, kind::decimal):
            return is_widening_convertible(model::decimal { decimal_precision_int1 }, target);
        case npair(kind::int2, kind::decimal):
            return is_widening_convertible(model::decimal { decimal_precision_int2 }, target);
        case npair(kind::int4, kind::decimal):
            return is_widening_convertible(model::decimal { decimal_precision_int4 }, target);
        case npair(kind::int8, kind::decimal):
            return is_widening_convertible(model::decimal { decimal_precision_int8 }, target);

        case npair(kind::decimal, kind::int1):
            return is_widening_convertible(type, model::decimal { decimal_precision_int1 - 1 });
        case npair(kind::decimal, kind::int2):
            return is_widening_convertible(type, model::decimal { decimal_precision_int2 - 1 });
        case npair(kind::decimal, kind::int4):
            return is_widening_convertible(type, model::decimal { decimal_precision_int4 - 1 });
        case npair(kind::decimal, kind::int8):
            return is_widening_convertible(type, model::decimal { decimal_precision_int8 - 1 });

        case npair(kind::decimal, kind::decimal): {
            auto&& a = unsafe_downcast<model::decimal>(type);
            auto&& b = unsafe_downcast<model::decimal>(target);
            if (a.scale() > b.scale()) return ternary::no;
            if (!b.precision()) return ternary::yes;
            if (!a.precision()) return ternary::no;
            auto ai = static_cast<std::int64_t>(*a.precision()) - a.scale();
            auto bi = static_cast<std::int64_t>(*b.precision()) - b.scale();
            return ternary_of(ai <= bi);
        }

        case npair(kind::character, kind::character): {
            auto&& a = unsafe_downcast<model::character>(type);
            auto&& b = unsafe_downcast<model::character>(target);
            if (a.varying() && !b.varying()) return ternary::no;
            if (!a.length()) return ternary::no;
            if (!b.length()) return ternary::yes;
            return ternary_of(*a.length() <= b.length());
        }

        case npair(kind::bit, kind::bit): {
            auto&& a = unsafe_downcast<model::bit>(type);
            auto&& b = unsafe_downcast<model::bit>(target);
            if (a.varying() && !b.varying()) return ternary::no;
            if (!a.length()) return ternary::no;
            if (!b.length()) return ternary::yes;
            return ternary_of(*a.length() <= b.length());
        }

        case npair(kind::array, kind::array):
        case npair(kind::record, kind::record):
        case npair(kind::declared, kind::declared):
            return ternary_of(type == target);

        case npair(kind::extension, kind::extension):
            return ternary_of(type == target);

        default:
            return ternary::no;
    }
}

} // namespace yugawara::type
