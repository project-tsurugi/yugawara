#include <yugawara/storage/partition/character_prefix.h>

#include <takatori/type/character.h>

#include <takatori/value/character.h>
#include <takatori/util/downcast.h>

#include <yugawara/extension/type/error.h>
#include <yugawara/extension/type/pending.h>

#include "details/prefix_extractor_util.h"

namespace yugawara::storage::partition {

using ::takatori::util::unsafe_downcast;

bool is_applicable(
        ::takatori::util::enum_tag_t<prefix_extractor_kind::character_prefix>,
        ::takatori::type::data const& source_type) noexcept {
    return details::is_parameter_applicable(source_type, ::takatori::type::character { ::takatori::type::varying });
}

std::shared_ptr<::takatori::type::data const> compute_type(
        character_prefix const& extractor,
        ::takatori::type::data const& source_type,
        ::yugawara::type::repository& repo) {
    if (details::is_stop_type(source_type)) {
        return repo.get(extension::type::pending {});
    }
    if (!is_applicable(::takatori::util::enum_tag<prefix_extractor_kind::character_prefix>, source_type)) {
        return repo.get(extension::type::error {});
    }
    (void) extractor;
    return repo.get(::takatori::type::character { ::takatori::type::varying });
}

bool is_valid_output(character_prefix const& extractor, ::takatori::value::data const& value) noexcept {
    if (details::is_null_value(value)) {
        return true;
    }
    if (value.kind() != ::takatori::value::value_kind::character) {
        return false;
    }
    auto&& string = unsafe_downcast<::takatori::value::character>(value).get();
    return details::count_characters(string) <= extractor.length();
}

} // namespace yugawara::storage::partition
