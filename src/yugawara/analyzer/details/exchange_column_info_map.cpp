#include "exchange_column_info_map.h"

#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>
#include <yugawara/binding/extract.h>

namespace yugawara::analyzer::details {

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

bool exchange_column_info_map::empty() const noexcept {
    return mappings_.empty();
}

exchange_column_info_map::size_type exchange_column_info_map::size() const noexcept {
    return mappings_.size();
}

static ::takatori::plan::exchange const* extract(::takatori::descriptor::relation const& relation) {
    if (auto exchange = binding::extract_if<::takatori::plan::exchange>(relation)) {
        return exchange.get();
    }
    throw_exception(std::invalid_argument(string_builder {}
            << "invalid exchange descriptor: "
            << relation
            << string_builder::to_string));
}

exchange_column_info_map::element_type& exchange_column_info_map::get(::takatori::plan::exchange const& declaration) {
    if (auto it = mappings_.find(std::addressof(declaration)); it != mappings_.end()) {
        return it.value();
    }
    throw_exception(std::invalid_argument(string_builder {}
            << "unregistered exchange: "
            << declaration
            << string_builder::to_string));
}

exchange_column_info_map::element_type& exchange_column_info_map::get(
        ::takatori::descriptor::relation const& relation) {
    return get(*extract(relation));
}

exchange_column_info_map::element_type& exchange_column_info_map::create_or_get(
        ::takatori::descriptor::relation const& relation) {
    auto [it, success] = mappings_.emplace(extract(relation), element_type {});
    (void) success;
    return it.value();
}

void exchange_column_info_map::erase(::takatori::descriptor::relation const& relation) {
    mappings_.erase(extract(relation));
}

bool exchange_column_info_map::is_target(::takatori::descriptor::relation const& relation) {
    return binding::kind_of(relation) == binding::relation_info_kind::exchange;
}

} // namespace yugawara::analyzer::details
