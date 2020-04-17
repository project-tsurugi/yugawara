#include "exchange_column_info_map.h"

#include <stdexcept>

#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>

namespace yugawara::analyzer::details {

using ::takatori::util::string_builder;

exchange_column_info_map::exchange_column_info_map(
        ::takatori::util::object_creator creator) noexcept
    : mappings_(creator.allocator())
{}

bool exchange_column_info_map::empty() const noexcept {
    return mappings_.empty();
}

exchange_column_info_map::size_type exchange_column_info_map::size() const noexcept {
    return mappings_.size();
}

static ::takatori::plan::exchange const* extract(::takatori::descriptor::relation const& relation) {
    auto&& info = binding::unwrap(relation);
    if (info.kind() == binding::exchange_info::tag) {
        auto&& decl = ::takatori::util::unsafe_downcast<binding::exchange_info>(info).declaration();
        return std::addressof(decl);
    }
    throw std::invalid_argument(string_builder {}
            << "invalid exchange descriptor: "
            << relation
            << string_builder::to_string);
}

exchange_column_info_map::element_type& exchange_column_info_map::get(::takatori::plan::exchange const& declaration) {
    if (auto it = mappings_.find(std::addressof(declaration)); it != mappings_.end()) {
        return it->second;
    }
    throw std::invalid_argument(string_builder {}
            << "unregistered exchange: "
            << declaration
            << string_builder::to_string);
}

exchange_column_info_map::element_type& exchange_column_info_map::get(
        ::takatori::descriptor::relation const& relation) {
    return get(*extract(relation));
}

exchange_column_info_map::element_type& exchange_column_info_map::create_or_get(
        ::takatori::descriptor::relation const& relation) {
    auto [it, success] = mappings_.emplace(extract(relation), get_object_creator());
    (void) success;
    return it->second;
}

void exchange_column_info_map::erase(::takatori::descriptor::relation const& relation) {
    mappings_.erase(extract(relation));
}

bool exchange_column_info_map::is_target(::takatori::descriptor::relation const& relation) {
    auto&& info = binding::unwrap(relation);
    return info.kind() == binding::exchange_info::tag;
}

} // namespace yugawara::analyzer::details
