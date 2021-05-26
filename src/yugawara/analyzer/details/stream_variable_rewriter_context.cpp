#include "stream_variable_rewriter_context.h"

#include <takatori/util/assertion.h>
#include <takatori/util/exception.h>
#include <takatori/util/string_builder.h>

#include <yugawara/binding/factory.h>
#include <yugawara/binding/extract.h>

#include <yugawara/binding/variable_info_impl.h>

namespace yugawara::analyzer::details {

using ::takatori::util::string_builder;
using ::takatori::util::throw_exception;

::takatori::util::optional_ptr<::takatori::descriptor::variable const>
stream_variable_rewriter_context::find(::takatori::descriptor::variable const& variable) const {
    if (auto it = mappings_.find(variable); it != mappings_.end()) {
        auto&& e = it->second;
        if (e.status == status_t::alias) {
            return find(e.variable);
        }
        return e.variable;
    }
    return {};
}

void stream_variable_rewriter_context::each_undefined(consumer_type const& consumer) {
    for (auto&& [v, info] : mappings_) {
        if (info.status == status_t::undefined) {
            consumer(v);
        }
    }
}

bool stream_variable_rewriter_context::try_rewrite_define(::takatori::descriptor::variable& variable) {
    if (auto it = mappings_.find(variable); it != mappings_.end()) {
        auto&& e = it.value();
        if (e.status != status_t::undefined) {
            throw_exception(std::domain_error(string_builder {}
                    << "redefine stream variable: "
                    << variable
                    << string_builder::to_string));
        }
        e.status = status_t::defined;
        variable = e.variable;
        return true;
    }
    return false;
}

void stream_variable_rewriter_context::rewrite_use(::takatori::descriptor::variable& variable) {
    using kind = binding::variable_info_kind;
    switch (binding::kind_of(variable)) {
        case kind::external_variable:
        case kind::frame_variable:
            break;

        case kind::stream_variable: {
            if (auto it = mappings_.find(variable); it != mappings_.end()) {
                auto&& e = it->second;
                switch (e.status) {
                    case status_t::undefined:
                        variable = e.variable;
                        break;
                    case status_t::defined:
                        throw_exception(std::domain_error(string_builder {}
                                << "use before define stream variable: "
                                << variable
                                << string_builder::to_string));
                    case status_t::alias:
                        variable = e.variable;
                        return rewrite_use(variable);
                }
                break;
            }
            auto&& info = binding::extract<kind::stream_variable>(variable);
            binding::factory f {};
            auto [it, success] = mappings_.emplace(
                    variable,
                    entry {
                            f.stream_variable(info.label()),
                            status_t::undefined,
                    });
            (void) success;
            BOOST_ASSERT(success); // NOLINT
            variable = it->second.variable;
            break;
        }

        default:
            throw_exception(std::invalid_argument(string_builder {}
                    << "unhandled variable: "
                    << variable
                    << string_builder::to_string));
    }
}

void stream_variable_rewriter_context::alias(
        ::takatori::descriptor::variable const& source,
        ::takatori::descriptor::variable const& alias) {
    auto [it, success] = mappings_.emplace(
            alias,
            entry {
                    source,
                    status_t::alias,
            });
    if (!success) {
        throw_exception(std::invalid_argument(string_builder {}
                << "duplicate entry for alias: "
                << alias
                << " : "
                << it->second.variable
                << string_builder::to_string));
    }
}

void stream_variable_rewriter_context::clear() {
    mappings_.clear();
}

} // namespace yugawara::analyzer::details
