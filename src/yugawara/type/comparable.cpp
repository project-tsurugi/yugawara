#include <yugawara/type/comparable.h>

#include <cstdlib>

#include <takatori/scalar/comparison_operator.h>

#include <takatori/type/data.h>
#include <takatori/type/type_kind.h>

namespace yugawara::type {

bool is_comparable(::takatori::scalar::comparison_operator comparison, ::takatori::type::data const& type) noexcept {
    using op = ::takatori::scalar::comparison_operator;
    switch (comparison) {
        case op::equal:
        case op::not_equal:
            return is_equality_comparable(type);
        case op::less:
        case op::less_equal:
        case op::greater:
        case op::greater_equal:
            return is_order_comparable(type);
    }
    std::abort();
}


bool is_equality_comparable(::takatori::type::data const& type) noexcept {
    auto kind = type.kind();
    using k = ::takatori::type::type_kind;
    switch (kind) {
        case k::boolean:
        case k::int1:
        case k::int2:
        case k::int4:
        case k::int8:
        case k::decimal:
        case k::float4:
        case k::float8:
        case k::character:
        case k::bit:
        case k::octet:
        case k::date:
        case k::time_of_day:
        case k::time_point:
        case k::datetime_interval:
        case k::array:
        case k::record:
            return true;

        case k::blob:
        case k::clob:
        case k::unknown:
        case k::row_id:
        case k::declared:
        default:
            return false;
    }
}

bool is_order_comparable(::takatori::type::data const& type) noexcept {
    auto kind = type.kind();
    using k = ::takatori::type::type_kind;
    switch (kind) {
        case k::boolean:
        case k::int1:
        case k::int2:
        case k::int4:
        case k::int8:
        case k::decimal:
        case k::float4:
        case k::float8:
        case k::character:
        case k::bit:
        case k::octet:
        case k::date:
        case k::time_of_day:
        case k::time_point:
            return true;

        case k::datetime_interval:
        case k::blob:
        case k::clob:
        case k::array:
        case k::record:
        case k::unknown:
        case k::row_id:
        case k::declared:
        default:
            return false;
    }
}

} // namespace yugawara::type
