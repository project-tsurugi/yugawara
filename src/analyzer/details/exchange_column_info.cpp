#include "exchange_column_info.h"

#include <stdexcept>
#include <cassert>

#include <takatori/util/downcast.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>

#include "../../binding/variable_info_impl.h"

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;

template<binding::variable_info_kind Kind>
static binding::variable_info_impl<Kind> const& check_variable(descriptor::variable const& variable) {
    using ::takatori::util::unsafe_downcast;
    constexpr auto info_kind = Kind;
    auto&& info = binding::unwrap(variable);
    if (info.kind() != info_kind) {
        using ::takatori::util::string_builder;
        throw std::invalid_argument(string_builder {}
                << "invalid " << info_kind << ":"
                << variable
                << string_builder::to_string);
    }
    return unsafe_downcast<binding::variable_info_impl<info_kind>>(info);
}

exchange_column_info::exchange_column_info(::takatori::util::object_creator creator) noexcept
    : entries_(creator.allocator<decltype(entries_)::value_type>())
    , index_(creator.allocator<decltype(index_)::value_type>())
    , touched_(creator.allocator<decltype(touched_)::value_type>())
{}

exchange_column_info::size_type exchange_column_info::count() const noexcept {
    return entries_.size();
}

::takatori::util::optional_ptr<descriptor::variable const>
exchange_column_info::find(descriptor::variable const& variable) const {
    if (auto it = index_.find(variable); it != index_.end()) {
        return entries_[it->second].second;
    }
    return {};
}

void exchange_column_info::each_mapping(mapping_consumer_type const& consumer) const {
    for (auto&& [origin, exchange_column] : entries_) {
        consumer(origin, exchange_column);
    }
}

descriptor::variable const& exchange_column_info::allocate(descriptor::variable const& variable) {
    if (auto v = find(variable)) {
        return *v;
    }
    auto&& info = check_variable<binding::variable_info_kind::stream_variable>(variable);
    auto f = binding::factory { get_object_creator() };
    auto&& entry = entries_.emplace_back(variable, f.exchange_column(info.label()));
    auto [it, success] = index_.emplace(variable, entries_.size() - 1);
    (void) it;
    assert(success); // NOLINT
    return entry.second;
}

void exchange_column_info::bind(
        descriptor::variable variable,
        descriptor::variable replacement) {
    check_variable<binding::variable_info_kind::stream_variable>(variable);
    check_variable<binding::variable_info_kind::exchange_column>(replacement);
    entries_.emplace_back(variable, std::move(replacement));
    auto [it, success] = index_.emplace(std::move(variable), entries_.size() - 1);
    assert(success); // NOLINT
    (void) it;
}

void exchange_column_info::touch(descriptor::variable const& variable) {
    check_variable<binding::variable_info_kind::exchange_column>(variable);
    touched_.emplace(variable);
}

bool exchange_column_info::is_touched(descriptor::variable const& variable) const {
    return touched_.find(variable) != touched_.end();
}

void exchange_column_info::clear_touched() {
    touched_.clear();
}

void exchange_column_info::clear() {
    entries_.clear();
    index_.clear();
    touched_.clear();
}

::takatori::util::object_creator exchange_column_info::get_object_creator() const noexcept {
    return entries_.get_allocator().resource();
}

} // namespace yugawara::analyzer::details