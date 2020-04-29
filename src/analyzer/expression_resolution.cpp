#include <yugawara/analyzer/expression_resolution.h>

#include <stdexcept>

namespace yugawara::analyzer {

using ::takatori::util::optional_ptr;

expression_resolution::expression_resolution(std::shared_ptr<::takatori::type::data const> type) noexcept
    : type_(std::move(type))
{}

expression_resolution::expression_resolution(takatori::type::data&& type, type::repository& repo)
    : expression_resolution(
            repo.get(std::move(type)))
{}

::takatori::type::data const& expression_resolution::type() const {
    check_resolved();
    return *type_;
}

optional_ptr<::takatori::type::data const> expression_resolution::optional_type() const noexcept {
    return optional_ptr { type_.get() };
}

std::shared_ptr<::takatori::type::data const> expression_resolution::shared_type() const noexcept {
    return type_;
}

bool expression_resolution::is_resolved() const noexcept {
    return type_ != nullptr;
}

expression_resolution::operator bool() const noexcept {
    return is_resolved();
}

void expression_resolution::check_resolved() const {
    if (!is_resolved()) {
        throw std::domain_error("unresolved expression");
    }
}

std::ostream& operator<<(std::ostream& out, expression_resolution const& value) {
    return out << "expression_resolution("
               << "type=" << value.optional_type() << ")";
}

bool operator==(expression_resolution const& a, expression_resolution const& b) {
    return a.optional_type() == b.optional_type();
}

bool operator!=(expression_resolution const& a, expression_resolution const& b) {
    return !(a == b);
}

} // namespace yugawara::analyzer
