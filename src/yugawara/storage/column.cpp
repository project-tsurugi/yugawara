#include <yugawara/storage/column.h>
#include <yugawara/storage/relation.h>

#include <takatori/util/clonable.h>

namespace yugawara::storage {

column::column(
        simple_name_type simple_name,
        std::shared_ptr<takatori::type::data const> type,
        variable::criteria criteria,
        column_value default_value) noexcept :
    simple_name_ { std::move(simple_name) },
    type_ { std::move(type) },
    criteria_ { std::move(criteria) },
    default_value_ { std::move(default_value) }
{}

column::column(
        std::string_view simple_name,
        takatori::type::data&& type,
        variable::criteria criteria,
        column_value default_value) :
    column {
        decltype(simple_name_) { simple_name },
        takatori::util::clone_shared(std::move(type)),
        std::move(criteria),
        std::move(default_value),
    }
{}

column::column(::takatori::util::clone_tag_t, column const& other)
    : column(
            decltype(simple_name_) { other.simple_name_ },
            other.type_,
            decltype(criteria_) { ::takatori::util::clone_tag, other.criteria_ },
            other.default_value_)
{}

column::column(::takatori::util::clone_tag_t, column&& other)
    : column(
            decltype(simple_name_) { std::move(other.simple_name_) },
            std::move(other.type_),
            std::move(other.criteria_),
            std::move(other.default_value_))
{}

column* column::clone() const& {
    return new column(::takatori::util::clone_tag, *this); // NOLINT
}

column* column::clone() && {
    return new column(::takatori::util::clone_tag, std::move(*this)); // NOLINT;
}

std::string_view column::simple_name() const noexcept {
    return simple_name_;
}

column& column::simple_name(simple_name_type simple_name) noexcept {
    simple_name_ = std::move(simple_name);
    return *this;
}

takatori::type::data const& column::type() const noexcept {
    return *type_;
}

takatori::util::optional_ptr<takatori::type::data const> column::optional_type() const noexcept {
    return takatori::util::optional_ptr<takatori::type::data const> { type_.get() };
}

std::shared_ptr<takatori::type::data const> column::shared_type() const noexcept {
    return type_;
}

column& column::type(std::shared_ptr<takatori::type::data const> type) noexcept {
    type_ = std::move(type);
    return *this;
}

variable::criteria& column::criteria() noexcept {
    return criteria_;
}

variable::criteria const& column::criteria() const noexcept {
    return criteria_;
}

column_value& column::default_value() noexcept {
    return default_value_;
}

column_value const& column::default_value() const noexcept {
    return default_value_;
}

relation const& column::owner() const noexcept {
    return *owner_;
}

takatori::util::optional_ptr<relation const> column::optional_owner() const noexcept {
    return takatori::util::optional_ptr<relation const> { owner_ };
}

std::ostream& operator<<(std::ostream& out, column const& value) {
    std::string_view parent { "(N/A)" };
    if (auto owner = value.optional_owner()) {
        parent = owner->simple_name();
    }
    return out << "column("
               << "owner=" << parent << ", "
               << "simple_name=" << value.simple_name() << ", "
               << "type=" << value.type() << ", "
               << "default_value=" << value.default_value() << ", "
               << "criteria=" << value.criteria() << ")";
}

void column::parent_element(relation* parent) noexcept {
    owner_ = parent;
}

} // namespace yugawara::storage
