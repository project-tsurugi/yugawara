#include <yugawara/variable/declaration.h>

#include <takatori/util/clonable.h>
#include <takatori/util/print_support.h>

#include <yugawara/type/extensions/pending.h>

namespace yugawara::variable {

static declaration::type_pointer or_pending(declaration::type_pointer ptr) {
    if (!ptr) {
        static auto pending = std::make_shared<type::extensions::pending>();
        return pending;
    }
    return ptr;
}

declaration::declaration(
        name_type name,
        type_pointer type,
        class criteria criteria) noexcept
    : name_(std::move(name))
    , type_(or_pending(std::move(type)))
    , criteria_(std::move(criteria))
{}

declaration::declaration(
        std::string_view name,
        takatori::util::rvalue_ptr<takatori::type::data> type,
        class criteria criteria)
    : declaration(
            decltype(name_) { name },
            takatori::util::clone_shared(type),
            std::move(criteria))
{}

bool declaration::is_resolved() const noexcept {
    return !type::extensions::pending::is_instance(*type_);
}

declaration::operator bool() const noexcept {
    return is_resolved();
}

std::string_view declaration::name() const noexcept {
    return name_;
}

declaration& declaration::name(declaration::name_type name) noexcept {
    name_ = std::move(name);
    return *this;
}

takatori::type::data const& declaration::type() const noexcept {
    return *type_;
}

declaration::type_pointer const& declaration::shared_type() const noexcept {
    return type_;
}

declaration& declaration::type(declaration::type_pointer type) noexcept {
    type_ = or_pending(std::move(type));
    return *this;
}

class criteria& declaration::criteria() noexcept {
    return criteria_;
}

class criteria const& declaration::criteria() const noexcept {
    return criteria_;
}

std::ostream& operator<<(std::ostream& out, declaration const& value) {
    return out << "variable("
               << "name=" << value.name() << ", "
               << "type=" << value.type() << ", "
               << "criteria=" << value.criteria() << ")";
}

} // namespace yugawara::variable
