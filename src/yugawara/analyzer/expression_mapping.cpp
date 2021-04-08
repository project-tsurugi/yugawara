#include <yugawara/analyzer/expression_mapping.h>

#include <takatori/util/clonable.h>
#include <takatori/util/exception.h>

namespace yugawara::analyzer {

using ::takatori::util::throw_exception;

expression_mapping::expression_mapping(::takatori::util::object_creator creator) noexcept
    : mapping_(creator.allocator())
{}

expression_resolution const& expression_mapping::find(
        ::takatori::scalar::expression const& expression) const {
    if (auto it = mapping_.find(std::addressof(expression)); it != mapping_.end()) {
        return it->second;
    }
    static expression_resolution const empty;
    return empty;
}

void expression_mapping::each(consumer_type const& consumer) const {
    for (auto&& [expression, resolution] : mapping_) {
        consumer(*expression, resolution);
    }
}

expression_resolution const& expression_mapping::bind(
        ::takatori::scalar::expression const& expression,
        expression_resolution resolution,
        bool overwrite) {
    if (overwrite) {
        auto [iter, success] = mapping_.insert_or_assign(std::addressof(expression), std::move(resolution));
        (void) success;
        return iter->second;
    }
    auto [iter, success] = mapping_.try_emplace(std::addressof(expression), std::move(resolution));
    if (!success && resolution != iter->second) { // NOLINT: try_emplace does not actually move if not success
        throw_exception(std::domain_error("rebind different type for the expression"));
    }
    return iter->second;
}

void expression_mapping::unbind(takatori::scalar::expression const& expression) {
    mapping_.erase(std::addressof(expression));
}

void expression_mapping::clear() noexcept {
    mapping_.clear();
}

takatori::util::object_creator expression_mapping::get_object_creator() const {
    return mapping_.get_allocator();
}

} // namespace yugawara::analyzer
