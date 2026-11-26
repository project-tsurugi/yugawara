#include <yugawara/storage/partition/floor_division.h>

#include <takatori/type/primitive.h>
#include <takatori/type/decimal.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

#include "details/prefix_extractor_util.h"

namespace yugawara::storage::partition {

bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::floor_division>,
        ::takatori::type::data const& source_type) noexcept {
    return details::is_parameter_applicable(source_type, ::takatori::type::decimal {});
}

std::shared_ptr<::takatori::type::data const> compute_type(
        floor_division const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo) {
    if (details::is_stop_type(source_type)) {
        return repo.get(extension::type::pending {});
    }
    if (!is_applicable(::takatori::util::enum_tag<prefix_extractor_kind::floor_division>, source_type)) {
        return repo.get(extension::type::error {});
    }
    (void) extractor;
    using t = ::takatori::type::type_kind;
    switch (source_type.kind()) {
        case t::int1:
        case t::int2:
        case t::int4:
            return repo.get(::takatori::type::int4 {});
        case t::int8:
            return repo.get(::takatori::type::int8 {});
        case t::decimal:
            return repo.get(::takatori::type::decimal {});
        default:
            break;
    }
    return repo.get(extension::type::error {});
}

bool is_valid_output(floor_division const& extractor, ::takatori::value::data const& value) noexcept {
    (void) extractor;
    return details::is_null_value(value) || details::is_exact_number_with_scale(value, 0);
}

} // namespace yugawara::storage::partition
