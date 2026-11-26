#include <yugawara/storage/partition/timestamp_truncate.h>

#include <takatori/type/primitive.h>
#include <takatori/type/date.h>
#include <takatori/type/time_point.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

#include "details/prefix_extractor_util.h"

namespace yugawara::storage::partition {

bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::timestamp_truncate>,
        ::takatori::type::data const& source_type) noexcept {
    return details::is_parameter_applicable(source_type, ::takatori::type::time_point {});
}

std::shared_ptr<::takatori::type::data const> compute_type(
        timestamp_truncate const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo) {
    if (details::is_stop_type(source_type)) {
        return repo.get(extension::type::pending {});
    }
    if (!is_applicable(::takatori::util::enum_tag<prefix_extractor_kind::timestamp_truncate>, source_type)) {
        return repo.get(extension::type::error {});
    }
    switch (extractor.unit()) {
        case timestamp_unit::year:
            return repo.get(::takatori::type::int4 {});
        case timestamp_unit::month:
        case timestamp_unit::day:
            return repo.get(::takatori::type::date {});
        case timestamp_unit::hour:
        case timestamp_unit::minute:
        case timestamp_unit::second:
            return repo.get(::takatori::type::time_point {});
    }
    return repo.get(extension::type::error {});
}

bool is_valid_output(timestamp_truncate const& extractor, ::takatori::value::data const& value) noexcept {
    if (details::is_null_value(value)) {
        return true;
    }
    switch (extractor.unit()) {
        case timestamp_unit::year:
            return details::is_exact_number_with_scale(value, 0);
        case timestamp_unit::month:
            return details::is_date_value(value, details::date_precision::month);
        case timestamp_unit::day:
            return details::is_date_value(value, details::date_precision::day);
        case timestamp_unit::hour:
            return details::is_time_point_value(value, details::time_point_precision::hour);
        case timestamp_unit::minute:
            return details::is_time_point_value(value, details::time_point_precision::minute);
        case timestamp_unit::second:
            return details::is_time_point_value(value, details::time_point_precision::second, extractor.second_scale());
    }
    return false;
}

} // namespace yugawara::storage::partition
