#include <yugawara/storage/partition/floor_fractional.h>

#include <takatori/type/decimal.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

#include "details/prefix_extractor_util.h"

namespace yugawara::storage::partition {

bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::floor_fractional>,
        ::takatori::type::data const& source_type) noexcept {
    return source_type.kind() == ::takatori::type::type_kind::decimal;
}

std::shared_ptr<::takatori::type::data const> compute_type(
        floor_fractional const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo) {
    if (details::is_stop_type(source_type)) {
        return repo.get(extension::type::pending {});
    }
    if (!is_applicable(::takatori::util::enum_tag<prefix_extractor_kind::floor_fractional>, source_type)) {
        return repo.get(extension::type::error {});
    }
    (void) extractor;
    return repo.get(::takatori::type::decimal {});
}

bool is_valid_output(floor_fractional const& extractor, ::takatori::value::data const& value) noexcept {
    return details::is_null_value(value) || details::is_exact_number_with_scale(value, extractor.scale());
}

} // namespace yugawara::storage::partition
