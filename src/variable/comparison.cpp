#include "yugawara/variable/comparison.h"

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>

namespace yugawara::variable {

comparison::comparison(
        operator_kind_type operator_kind,
        std::shared_ptr<takatori::value::data const> value) noexcept
    : operator_kind_(operator_kind)
    , value_(std::move(value))
{}

comparison::comparison(comparison::operator_kind_type operator_kind, takatori::value::data&& value)
    : comparison(
            operator_kind,
            takatori::util::clone_unique(std::move(value)))
{}

comparison::comparison(comparison const& other, takatori::util::object_creator) noexcept
    : comparison(other.operator_kind_, other.value_)
{}

comparison::comparison(comparison&& other, takatori::util::object_creator) noexcept
    : comparison(other.operator_kind_, std::move(other.value_))
{}

predicate_kind comparison::kind() const noexcept {
    return tag;
}

comparison* comparison::clone(takatori::util::object_creator creator) const& {
    return creator.create_object<comparison>(*this, creator);
}

comparison* comparison::clone(takatori::util::object_creator creator)&& {
    return creator.create_object<comparison>(std::move(*this), creator);
}

comparison::operator_kind_type comparison::operator_kind() const noexcept {
    return operator_kind_;
}

comparison& comparison::operator_kind(comparison::operator_kind_type operator_kind) noexcept {
    operator_kind_ = operator_kind;
    return *this;
}

takatori::value::data const& comparison::value() const noexcept {
    return *value_;
}

takatori::util::optional_ptr<takatori::value::data const> comparison::optional_value() const noexcept {
    return takatori::util::optional_ptr<takatori::value::data const> { value_.get() };
}

std::shared_ptr<takatori::value::data const> comparison::shared_value() const noexcept {
    return value_;
}

comparison& comparison::value(std::shared_ptr<takatori::value::data const> value) noexcept {
    value_ = std::move(value);
    return *this;
}

bool operator==(comparison const& a, comparison const& b) noexcept {
    return a.operator_kind() == b.operator_kind()
        && a.optional_value() == b.optional_value();
}

bool operator!=(comparison const& a, comparison const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, comparison const& value) {
    return out << value.kind() << "("
               << "operator_kind=" << value.operator_kind() << ", "
               << "value=" << value.optional_value() << ")";
}

bool comparison::equals(predicate const& other) const noexcept {
    return tag == other.kind() && *this == takatori::util::unsafe_downcast<comparison>(other);
}

std::ostream& comparison::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::variable
