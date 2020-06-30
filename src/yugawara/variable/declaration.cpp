#include <yugawara/variable/declaration.h>

#include <takatori/util/clonable.h>
#include <takatori/util/optional_print_support.h>

#include <yugawara/extension/type/pending.h>

namespace yugawara::variable {

static declaration::type_pointer or_pending(declaration::type_pointer ptr) {
    if (!ptr) {
        static auto pending = std::make_shared<extension::type::pending>();
        return pending;
    }
    return ptr;
}

declaration::declaration(
        std::optional<definition_id_type> definition_id,
        name_type name,
        type_pointer type,
        class criteria criteria) noexcept :
    definition_id_ { std::move(definition_id) },
    name_(std::move(name)),
    type_(or_pending(std::move(type))),
    criteria_(std::move(criteria))
{}

declaration::declaration(
        name_type name,
        type_pointer type,
        class criteria criteria) noexcept :
    declaration {
            std::nullopt,
            std::move(name),
            std::move(type),
            std::move(criteria),
    }
{}

declaration::declaration(
        std::string_view name,
        takatori::util::rvalue_ptr<takatori::type::data> type,
        class criteria criteria) :
    declaration {
            std::nullopt,
            decltype(name_) { name },
            takatori::util::clone_shared(type),
            std::move(criteria),
    }
{}

bool declaration::is_resolved() const noexcept {
    return !extension::type::pending::is_instance(*type_);
}

declaration::operator bool() const noexcept {
    return is_resolved();
}

std::optional<declaration::definition_id_type> declaration::definition_id() const noexcept {
    return definition_id_;
}

declaration& declaration::definition_id(std::optional<definition_id_type> definition_id) noexcept {
    definition_id_ = std::move(definition_id);
    return *this;
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

::takatori::util::optional_ptr<::takatori::type::data const> declaration::optional_type() const noexcept {
    return takatori::util::optional_ptr { type_.get() };
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
    using ::takatori::util::print_support;
    return out << "variable("
               << "definition_id=" << print_support { value.definition_id() } << ", "
               << "name=" << value.name() << ", "
               << "type=" << value.optional_type() << ", "
               << "criteria=" << value.criteria() << ")";
}

} // namespace yugawara::variable
