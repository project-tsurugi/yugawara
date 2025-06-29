#include <yugawara/type/conversion.h>

#include <algorithm>
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
using ::takatori::type::with_time_zone_t;

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

namespace {

constexpr std::uint64_t npair(kind a, kind b) noexcept {
    using utype = std::underlying_type_t<kind>;
    static_assert(sizeof(utype) <= sizeof(std::uint64_t) / 2);
    auto scale = static_cast<std::uint64_t>(std::numeric_limits<utype>::max()) + 1;
    return static_cast<std::uint64_t>(a) * scale + static_cast<std::uint64_t>(b);
}

std::shared_ptr<extension::type::error> shared_error() {
    static auto result = std::make_shared<extension::type::error>();
    return result;
}

std::shared_ptr<extension::type::pending> shared_pending() {
    static auto result = std::make_shared<extension::type::pending>();
    return result;
}

with_time_zone_t get_time_zone(model::data const& type) {
    switch (type.kind()) {
        case model::time_of_day::tag:
            return with_time_zone_t { unsafe_downcast<model::time_of_day>(type).with_time_zone() };
        case model::time_point::tag:
            return with_time_zone_t { unsafe_downcast<model::time_point>(type).with_time_zone() };
        default:
            std::abort();
    }
}

with_time_zone_t promote_time_zone(model::data const& a, model::data const& b) {
    auto&& tz1 = get_time_zone(a);
    auto&& tz2 = get_time_zone(b);
    if (tz1 == tz2) {
        return tz1;
    }
    return with_time_zone_t { true };
}

std::shared_ptr<model::data const> unary_external_promotion(
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

std::shared_ptr<model::data const> binary_external_promotion(
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

std::optional<category> unify_category(category a, category b) {
    // unifying conversion is defined only if they are in the same category, except special cases.
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

} // namespace

std::shared_ptr<model::data const> identity_conversion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    return repo.get(type);
}

static std::shared_ptr<model::data const> identity_conversion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    if (type.kind() == kind::unknown) {
        return repo.get(with);
    }
    if (with.kind() == kind::unknown) {
        return repo.get(type);
    }
    if (type == with) {
        return repo.get(type);
    }
    return shared_error();
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
            return binary_numeric_promotion(model::decimal { decimal_precision_int4, 0 }, with);
        case npair(kind::int1, kind::float4):
            return repo.get(model::float8 {});
        case npair(kind::int1, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::int2, kind::int1):
        case npair(kind::int2, kind::int2):
        case npair(kind::int2, kind::int4):
            return repo.get(model::int4 {});
        case npair(kind::int2, kind::int8):
            return repo.get(model::int8 {});
        case npair(kind::int2, kind::decimal):
            return binary_numeric_promotion(model::decimal { decimal_precision_int4, 0 }, with);
        case npair(kind::int2, kind::float4):
            return repo.get(model::float8 {});
        case npair(kind::int2, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::int4, kind::int1):
        case npair(kind::int4, kind::int2):
        case npair(kind::int4, kind::int4):
            return repo.get(model::int4 {});
        case npair(kind::int4, kind::int8):
            return repo.get(model::int8 {});
        case npair(kind::int4, kind::decimal):
            return binary_numeric_promotion(model::decimal { decimal_precision_int4, 0 }, with);
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
            return binary_numeric_promotion(model::decimal { decimal_precision_int8, 0 }, with);
        case npair(kind::int8, kind::float4):
            return repo.get(model::float8 {});
        case npair(kind::int8, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::decimal, kind::int1):
            return binary_numeric_promotion(type, model::decimal { decimal_precision_int4, 0 });
        case npair(kind::decimal, kind::int2):
            return binary_numeric_promotion(type, model::decimal { decimal_precision_int4, 0 });
        case npair(kind::decimal, kind::int4):
            return binary_numeric_promotion(type, model::decimal { decimal_precision_int4, 0 });
        case npair(kind::decimal, kind::int8):
            return binary_numeric_promotion(type, model::decimal { decimal_precision_int8, 0 });
        case npair(kind::decimal, kind::decimal):
        {
            auto&& a = unsafe_downcast<model::decimal>(type);
            auto&& b = unsafe_downcast<model::decimal>(with);
            std::optional<std::size_t> precision {};
            std::optional<std::size_t> scale {};
            if (a.scale() == b.scale()) {
                if (a.precision() == b.precision()) {
                    precision = a.precision();
                } else if (a.precision() && b.precision()) {
                    precision = std::max(*a.precision(), *b.precision());
                }
                scale = a.scale();
            }
            return repo.get(model::decimal { precision, scale });
        }
        case npair(kind::decimal, kind::float4):
            return repo.get(model::float8 {});
        case npair(kind::decimal, kind::float8):
            return repo.get(model::float8 {});

        case npair(kind::float4, kind::int1):
        case npair(kind::float4, kind::int2):
        case npair(kind::float4, kind::int4):
        case npair(kind::float4, kind::int8):
        case npair(kind::float4, kind::decimal):
            return repo.get(model::float8 {});
        case npair(kind::float4, kind::float4):
            return repo.get(model::float4 {});
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
        {
            auto&& a = unsafe_downcast<model::character>(type);
            auto&& b = unsafe_downcast<model::character>(with);
            if (a.length() == b.length()) {
                return repo.get(model::character { model::varying, a.length() });
            }
            return repo.get(model::character { model::varying, {} });
        }

        case npair(kind::character, kind::unknown):
            return unary_character_string_promotion(type, repo);

        case npair(kind::unknown, kind::character):
            return unary_character_string_promotion(with, repo);

        case npair(kind::unknown, kind::unknown):
            return unary_character_string_promotion(type, repo);

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const>
unary_octet_string_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::octet:
            // FIXME: always varying?
            return repo.get(model::octet {
                    model::varying,
                    unsafe_downcast<model::octet>(type).length(),
            });
        case kind::unknown:
            // unknown -> zero-length octet string
            return repo.get(model::octet {
                    model::varying,
                    0,
            });
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> binary_octet_string_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        // FIXME: always varying?
        case npair(kind::octet, kind::octet):
        {
            auto&& a = unsafe_downcast<model::octet>(type);
            auto&& b = unsafe_downcast<model::octet>(with);
            if (a.length() == b.length()) {
                return repo.get(model::octet { model::varying, a.length() });
            }
            return repo.get(model::octet { model::varying, {} });
        }

        case npair(kind::octet, kind::unknown):
            return unary_octet_string_promotion(type, repo);

        case npair(kind::unknown, kind::octet):
            return unary_octet_string_promotion(with, repo);

        case npair(kind::unknown, kind::unknown):
            return unary_octet_string_promotion(type, repo);

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

std::shared_ptr<model::data const> unary_large_octet_string_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::blob:
            return repo.get(type);
        case kind::unknown:
            return repo.get(model::blob {});
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> binary_large_octet_string_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        case npair(kind::blob, kind::blob):
        case npair(kind::blob, kind::unknown):
            return repo.get(model::blob {});

        case npair(kind::unknown, kind::blob):
        case npair(kind::unknown, kind::unknown):
            return repo.get(model::blob {});

        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> unary_large_character_string_promotion(model::data const& type, repository& repo) {
    if (is_conversion_stop_type(type)) return shared_pending();
    switch (type.kind()) {
        case kind::clob:
            return repo.get(type);
        case kind::unknown:
            return repo.get(model::clob {});
        default:
            return shared_error();
    }
}

std::shared_ptr<model::data const> binary_large_character_string_promotion(
        model::data const& type,
        model::data const& with,
        repository& repo) {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(with)) return shared_pending();
    switch (npair(type.kind(), with.kind())) {
        case npair(kind::clob, kind::clob):
        case npair(kind::clob, kind::unknown):
            return repo.get(model::clob {});

        case npair(kind::unknown, kind::clob):
        case npair(kind::unknown, kind::unknown):
            return repo.get(model::clob {});

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

        case category::octet_string:
            return unary_octet_string_promotion(type, repo);

        case category::bit_string:
            return unary_bit_string_promotion(type, repo);

        case category::temporal:
            return unary_temporal_promotion(type, repo);

        case category::datetime_interval:
            return unary_time_interval_promotion(type, repo);

        case category::large_octet_string:
            return unary_large_octet_string_promotion(type, repo);

        case category::large_character_string:
            return unary_large_character_string_promotion(type, repo);

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
            return binary_boolean_promotion(type, with, repo);

        case category::number:
            return binary_numeric_promotion(type, with, repo);

        case category::character_string:
            return binary_character_string_promotion(type, with, repo);

        case category::octet_string:
            return binary_octet_string_promotion(type, with, repo);

        case category::bit_string:
            return binary_bit_string_promotion(type, with, repo);

        case category::temporal:
            // NOTE: temporal types only allows identity conversions
            return identity_conversion(type, with, repo);

        case category::datetime_interval:
            return binary_time_interval_promotion(type, with, repo);

        case category::large_octet_string:
            return binary_large_octet_string_promotion(type, with, repo);

        case category::large_character_string:
            return binary_large_character_string_promotion(type, with, repo);

        case category::collection:
        case category::structure:
        case category::unique:
            return identity_conversion(type, with, repo);

        case category::external:
            return binary_external_promotion(type, with, repo);

        case category::unresolved:
            return shared_pending();
    }
    std::abort();
}

ternary is_assignment_convertible(model::data const& type, model::data const& target) noexcept {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(target)) return ternary::unknown;

    // can convert unknown to anything
    if (type.kind() == kind::unknown) {
        return ternary::yes;
    }

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
            return ternary::yes;

        case npair(kind::float4, kind::int1):
        case npair(kind::float4, kind::int2):
        case npair(kind::float4, kind::int4):
        case npair(kind::float4, kind::int8):
        case npair(kind::float4, kind::decimal):
            return ternary::no;

        case npair(kind::float4, kind::float4):
        case npair(kind::float4, kind::float8):
            return ternary::yes;

        case npair(kind::float8, kind::int1):
        case npair(kind::float8, kind::int2):
        case npair(kind::float8, kind::int4):
        case npair(kind::float8, kind::int8):
        case npair(kind::float8, kind::decimal):
            return ternary::no;

        case npair(kind::float8, kind::float4):
        case npair(kind::float8, kind::float8):
            return ternary::yes;

        case npair(kind::character, kind::character):

        case npair(kind::octet, kind::octet):

        case npair(kind::bit, kind::bit):

        case npair(kind::date, kind::date):
        case npair(kind::date, kind::time_point):
            return ternary::yes;

        case npair(kind::time_of_day, kind::time_of_day):
        case npair(kind::time_of_day, kind::time_point):
            if (get_time_zone(type) == get_time_zone(target)) {
                return ternary::yes;
            }
            return ternary::no;

        case npair(kind::time_point, kind::date):
            return ternary::yes;

        case npair(kind::time_point, kind::time_of_day):
        case npair(kind::time_point, kind::time_point):
            if (get_time_zone(type) == get_time_zone(target)) {
                return ternary::yes;
            }
            return ternary::no;

        case npair(kind::datetime_interval, kind::datetime_interval):
        case npair(kind::blob, kind::blob):
        case npair(kind::clob, kind::clob):
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

    // always cast convertible from/to character strings ..
    if (src_cat == category::character_string || dst_cat == category::character_string) {
        // .. except for converting from large octet strings
        if (src_cat == category::large_octet_string) {
            return ternary::no;
        }
        return ternary::yes;
    }

    switch (npair(type.kind(), target.kind())) {
        // allow approx. -> exact numbers
        case npair(kind::float4, kind::int1):
        case npair(kind::float4, kind::int2):
        case npair(kind::float4, kind::int4):
        case npair(kind::float4, kind::int8):
        case npair(kind::float4, kind::decimal):
        case npair(kind::float8, kind::int1):
        case npair(kind::float8, kind::int2):
        case npair(kind::float8, kind::int4):
        case npair(kind::float8, kind::int8):
        case npair(kind::float8, kind::decimal):

        // allow time_of_day <-> time_point with any time zone
        case npair(kind::time_of_day, kind::time_of_day):
        case npair(kind::time_of_day, kind::time_point):
        case npair(kind::time_point, kind::time_of_day):
        case npair(kind::time_point, kind::time_point):

        // allow octet <-> blob
        case npair(kind::octet, kind::blob):
        case npair(kind::blob, kind::octet):
            return ternary::yes;

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

util::ternary is_most_upperbound_compatible_type(const takatori::type::data &type) noexcept {
    if (is_conversion_stop_type(type)) return ternary::unknown;

    switch (type.kind()) {
        case kind::int1:
        case kind::int2:
            return util::ternary::no;

        case kind::decimal:
        {
            auto&& t = unsafe_downcast<model::decimal>(type);
            return util::ternary_of(t.precision() == std::nullopt && t.scale() == std::nullopt);
        }

        case kind::character:
        {
            auto&& t = unsafe_downcast<model::character>(type);
            return util::ternary_of(t.varying() && t.length() == std::nullopt);
        }

        case kind::bit:
        {
            auto&& t = unsafe_downcast<model::bit>(type);
            return util::ternary_of(t.varying() && t.length() == std::nullopt);
        }

        case kind::octet:
        {
            auto&& t = unsafe_downcast<model::octet>(type);
            return util::ternary_of(t.varying() && t.length() == std::nullopt);
        }

        default:
            return ternary::yes;
    }
}

util::ternary is_parameter_application_convertible(model::data const& type, model::data const& target) noexcept {
    if (is_conversion_stop_type(type) || is_conversion_stop_type(target)) return ternary::unknown;

    if (is_most_upperbound_compatible_type(target) == util::ternary::no) {
        return ternary::unknown;
    }

    // can convert unknown to anything
    if (type.kind() == kind::unknown) {
        return ternary::yes;
    }

    switch (npair(type.kind(), target.kind())) {
        case npair(kind::boolean, kind::boolean):
            return util::ternary::yes;

        case npair(kind::int1, kind::int1):
        case npair(kind::int1, kind::int2):
        case npair(kind::int1, kind::int4):
        case npair(kind::int1, kind::int8):
        case npair(kind::int1, kind::decimal):
        case npair(kind::int1, kind::float4):
        case npair(kind::int1, kind::float8):
            return util::ternary::yes;

        case npair(kind::int2, kind::int2):
        case npair(kind::int2, kind::int4):
        case npair(kind::int2, kind::int8):
        case npair(kind::int2, kind::decimal):
        case npair(kind::int2, kind::float4):
        case npair(kind::int2, kind::float8):
            return util::ternary::yes;

        case npair(kind::int4, kind::int4):
        case npair(kind::int4, kind::int8):
        case npair(kind::int4, kind::decimal):
        case npair(kind::int4, kind::float4):
        case npair(kind::int4, kind::float8):
            return util::ternary::yes;

        case npair(kind::int8, kind::int8):
        case npair(kind::int8, kind::decimal):
        case npair(kind::int8, kind::float4):
        case npair(kind::int8, kind::float8):
            return util::ternary::yes;

        case npair(kind::decimal, kind::decimal):
        case npair(kind::decimal, kind::float4):
        case npair(kind::decimal, kind::float8):
            return util::ternary::yes;

        case npair(kind::float4, kind::float4):
        case npair(kind::float4, kind::float8):
            return util::ternary::yes;

        case npair(kind::float8, kind::float8):
            return util::ternary::yes;

        case npair(kind::character, kind::character):
            return util::ternary::yes;

        case npair(kind::bit, kind::bit):
            return util::ternary::yes;

        case npair(kind::octet, kind::octet):
            return util::ternary::yes;

        case npair(kind::date, kind::date):
            return util::ternary::yes;

        case npair(kind::time_of_day, kind::time_of_day):
        {
            auto&& a = unsafe_downcast<model::time_of_day>(type);
            auto&& b = unsafe_downcast<model::time_of_day>(target);
            return util::ternary_of(a.with_time_zone() == b.with_time_zone());
        }

        case npair(kind::time_point, kind::time_point):
        {
            auto&& a = unsafe_downcast<model::time_point>(type);
            auto&& b = unsafe_downcast<model::time_point>(target);
            return util::ternary_of(a.with_time_zone() == b.with_time_zone());
        }

        // FIXME: interval type

        case npair(kind::blob, kind::blob):
            return util::ternary::yes;

        case npair(kind::clob, kind::clob):
            return util::ternary::yes;

        // FIXME: more types
        default:
            return util::ternary::no;
    }
}

} // namespace yugawara::type
