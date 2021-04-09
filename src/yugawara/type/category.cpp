#include <yugawara/type/category.h>

#include <takatori/type/type_kind.h>
#include <takatori/type/extension.h>
#include <takatori/util/downcast.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

namespace yugawara::type {

inline category extension_category_of(takatori::type::extension const& type) noexcept;

category category_of(takatori::type::data const& type) noexcept {
    using kind = takatori::type::type_kind;
    switch (type.kind()) {
        case kind::boolean:
            return category::boolean;

        case kind::int1:
        case kind::int2:
        case kind::int4:
        case kind::int8:
        case kind::float4:
        case kind::float8:
        case kind::decimal:
            return category::number;

        case kind::character:
            return category::character_string;

        case kind::octet:
            return category::octet_string;

        case kind::bit:
            return category::bit_string;

        case kind::date:
        case kind::time_of_day:
        case kind::time_point:
            return category::temporal;

        case kind::datetime_interval:
            return category::datetime_interval;

        case kind::array:
            return category::collection;

        case kind::record:
            return category::structure;

        case kind::unknown:
            return category::unknown;

        case kind::row_reference:
        case kind::row_id:
            return category::unique;

        case kind::declared:
            // FIXME: this prevents subtyping or implicit conversions
            return category::unique;

        case kind::extension:
            return extension_category_of(takatori::util::unsafe_downcast<takatori::type::extension>(type));
    }
    std::abort();
}

inline category extension_category_of(takatori::type::extension const& type) noexcept {
    if (extension::type::error::is_instance(type)
            || extension::type::pending::is_instance(type)) {
        return category::unresolved;
    }
    return category::external;
}

} // namespace yugawara::type
