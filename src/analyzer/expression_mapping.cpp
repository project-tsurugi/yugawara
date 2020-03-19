#include <yugawara/analyzer/expression_mapping.h>

#include <takatori/util/clonable.h>

namespace yugawara::analyzer {

expression_mapping::expression_mapping(::takatori::util::object_creator creator) noexcept
    : mapping_(creator.allocator<decltype(mapping_)::value_type>())
{}

std::shared_ptr<::takatori::type::data const> expression_mapping::find(
        ::takatori::scalar::expression const& expression) const {
    if (auto it = mapping_.find(std::addressof(expression)); it != mapping_.end()) {
        return it->second;
    }
    return {};
}

std::shared_ptr<::takatori::type::data const> const& expression_mapping::bind(
        ::takatori::scalar::expression const& expression,
        std::shared_ptr<::takatori::type::data const> type,
        bool overwrite) {
    auto&& type_ref = *type;
    if (overwrite) {
        auto [iter, success] = mapping_.insert_or_assign(std::addressof(expression), std::move(type));
        (void) success;
        return iter->second;
    }
    auto [iter, success] = mapping_.emplace(std::addressof(expression), std::move(type));
    if (!success && *iter->second != type_ref) {
        throw std::domain_error("rebind different type for the expression");
    }
    return iter->second;
}

std::shared_ptr<::takatori::type::data const> const& expression_mapping::bind(
        ::takatori::scalar::expression const& expression,
        ::takatori::type::data&& type,
        bool overwrite) {
    return bind(expression, ::takatori::util::clone_shared(std::move(type)), overwrite);
}

takatori::util::object_creator expression_mapping::get_object_creator() const {
    return mapping_.get_allocator();
}

} // namespace yugawara::analyzer
