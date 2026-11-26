#include <yugawara/storage/partition/date_truncate.h>

#include <takatori/type/primitive.h>
#include <takatori/type/date.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

#include "details/prefix_extractor_util.h"

namespace yugawara::storage::partition {

bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::date_truncate>,
        ::takatori::type::data const& source_type) noexcept {
    return details::is_parameter_applicable(source_type, ::takatori::type::date {});
}

std::shared_ptr<::takatori::type::data const> compute_type(
        date_truncate const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo) {
    if (details::is_stop_type(source_type)) {
        return repo.get(extension::type::pending {});
    }
    if (!is_applicable(::takatori::util::enum_tag<prefix_extractor_kind::date_truncate>, source_type)) {
        return repo.get(extension::type::error {});
    }
    switch (extractor.unit()) {
        case date_unit::year:
            return repo.get(::takatori::type::int4 {});
        case date_unit::month:
        case date_unit::day:
            return repo.get(::takatori::type::date {});
    }
    return repo.get(extension::type::error {});
}

} // namespace yugawara::storage::partition
