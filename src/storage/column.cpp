#include "yugawara/storage/column.h"
#include "yugawara/storage/relation.h"

#include <takatori/util/clonable.h>

namespace yugawara::storage {

column::column(
        simple_name_type simple_name,
        std::shared_ptr<takatori::type::data const> type,
        variable::criteria criteria,
        std::shared_ptr<takatori::value::data const> default_value) noexcept
    : simple_name_(std::move(simple_name))
    , type_(std::move(type))
    , criteria_(std::move(criteria))
    , default_value_(std::move(default_value))
{}

column::column(
        std::string_view simple_name,
        takatori::type::data&& type,
        variable::criteria criteria,
        takatori::util::rvalue_ptr<takatori::value::data> default_value)
    : column(
            decltype(simple_name_) { simple_name },
            takatori::util::clone_shared(std::move(type)),
            std::move(criteria),
            takatori::util::clone_shared(default_value))
{}

column::column(column const& other, takatori::util::object_creator creator)
    : column(
            decltype(simple_name_) { other.simple_name_, creator.allocator<char>() },
            other.type_,
            decltype(criteria_) { other.criteria_, creator },
            other.default_value_)
{}

column::column(column&& other, takatori::util::object_creator creator)
    : column(
            decltype(simple_name_) { std::move(other.simple_name_), creator.allocator<char>() },
            std::move(other.type_),
            std::move(other.criteria_),
            std::move(other.default_value_))
{}

column* column::clone(takatori::util::object_creator creator) const& {
    return creator.create_object<column>(*this, creator);
}

column* column::clone(takatori::util::object_creator creator) && {
    return creator.create_object<column>(std::move(*this), creator);
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

takatori::util::optional_ptr<takatori::value::data const> column::default_value() const noexcept {
    return takatori::util::optional_ptr<takatori::value::data const> { default_value_.get() };
}

std::shared_ptr<takatori::value::data const> column::shared_default_value() const noexcept {
    return default_value_;
}

column& column::default_value(std::shared_ptr<takatori::value::data const> default_value) noexcept {
    default_value_ = std::move(default_value);
    return *this;
}

relation const& column::owner() const noexcept {
    return *owner_;
}

takatori::util::optional_ptr<relation const> column::optional_owner() const noexcept {
    return takatori::util::optional_ptr<relation const> { owner_ };
}

std::ostream& operator<<(std::ostream& out, column const& value) {
    return out << "column("
               << "type=" << value.type() << ", "
               << "default_value=" << value.default_value() << ", "
               << "criteria=" << value.criteria() << ")";
}

void column::parent_element(relation* parent) noexcept {
    owner_ = parent;
}

} // namespace yugawara::storage
