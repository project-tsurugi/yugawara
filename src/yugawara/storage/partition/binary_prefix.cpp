#include <yugawara/storage/partition/binary_prefix.h>

#include <takatori/type/octet.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

#include "details/prefix_extractor_util.h"

namespace yugawara::storage::partition {

bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::binary_prefix>,
        ::takatori::type::data const& source_type) noexcept {
    return details::is_parameter_applicable(source_type, ::takatori::type::octet { ::takatori::type::varying });
}

std::shared_ptr<::takatori::type::data const> compute_type(
        binary_prefix const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo) {
    if (details::is_stop_type(source_type)) {
        return repo.get(extension::type::pending {});
    }
    if (!is_applicable(::takatori::util::enum_tag<prefix_extractor_kind::binary_prefix>, source_type)) {
        return repo.get(extension::type::error {});
    }
    (void) extractor;
    return repo.get(::takatori::type::octet { ::takatori::type::varying });
}

} // namespace yugawara::storage::partition
