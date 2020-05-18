#include <yugawara/analyzer/variable_mapping.h>

#include <takatori/util/exception.h>

namespace yugawara::analyzer {

using ::takatori::util::throw_exception;

variable_mapping::variable_mapping(::takatori::util::object_creator creator) noexcept
    : mapping_(creator.allocator())
{}

variable_resolution const& variable_mapping::find(::takatori::descriptor::variable const& variable) const {
    if (auto it = mapping_.find(variable); it != mapping_.end()) {
        return it->second;
    }
    static variable_resolution unresolved {};
    return unresolved;
}

variable_resolution const& variable_mapping::bind(
        ::takatori::descriptor::variable const& variable,
        variable_resolution resolution,
        bool overwrite) {
    if (overwrite) {
        auto [iter, success] = mapping_.insert_or_assign(variable, std::move(resolution));
        (void) success;
        return iter->second;
    }
    if (auto [iter, success] = mapping_.emplace(variable, std::move(resolution)); success) {
        return iter->second;
    }
    throw_exception(std::domain_error("rebind variable"));
}

void variable_mapping::unbind(takatori::descriptor::variable const& variable) {
    mapping_.erase(variable);
}

void variable_mapping::clear() noexcept {
    mapping_.clear();
}

takatori::util::object_creator variable_mapping::get_object_creator() const {
    return mapping_.get_allocator();
}

} // namespace yugawara::analyzer
