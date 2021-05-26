#include "stream_variable_flow_info_entry.h"

#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

namespace yugawara::analyzer::details {

namespace descriptor = ::takatori::descriptor;
namespace relation = ::takatori::relation;

using ::takatori::util::optional_ptr;
using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

optional_ptr<relation::expression const> stream_variable_flow_info_entry::originator() const noexcept {
    return originator_;
}

void stream_variable_flow_info_entry::originator(optional_ptr<relation::expression const> originator) noexcept {
    originator_ = originator;
}

bool stream_variable_flow_info_entry::is_separator() const noexcept {
    if (originator_) {
        return originator_->kind() == relation::expression_kind::escape;
    }
    return true;
}

optional_ptr<relation::expression const> stream_variable_flow_info_entry::find(descriptor::variable const& variable) const {
    if (auto it = declarations_.find(variable); it != declarations_.end()) {
        return optional_ptr { it.value() };
    }
    return {};
}

void stream_variable_flow_info_entry::each(consumer_type const& consumer) const {
    for (auto&& [variable, declarator] : declarations_) {
        (void) declarator;
        consumer(variable);
    }
}

void stream_variable_flow_info_entry::declare(descriptor::variable variable, relation::expression const& declaration) {
    if (auto [it, success] = declarations_.emplace(std::move(variable), std::addressof(declaration)); !success) {
        throw_exception(std::domain_error(string_builder {}
                << "already declared: "
                << "variable=" << variable << ", "
                << "declaration=" << *it.value()
                << string_builder::to_string));
    }
}

} // namespace yugawara::analyzer::details
