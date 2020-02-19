#include <yugawara/variable/quantification.h>

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>

namespace yugawara::variable {

quantification::quantification(
        operator_kind_type operator_kind,
        takatori::util::reference_vector<predicate> operands) noexcept
    : operator_kind_(operator_kind)
    , operands_(std::move(operands))
{}

quantification::quantification(operator_kind_type operator_kind, predicate&& left, predicate&& right)
    : quantification(
            operator_kind,
            { std::move(left), std::move(right) })
{}

quantification::quantification(
        operator_kind_type operator_kind,
        std::initializer_list<takatori::util::rvalue_reference_wrapper<predicate>> operands)
    : quantification(
            operator_kind,
            decltype(operands_) { operands.begin(), operands.end() })
{}

quantification::quantification(quantification const& other, takatori::util::object_creator creator) noexcept
    : quantification(
            other.operator_kind_,
            decltype(operands_) { other.operands_, creator })
{}

quantification::quantification(quantification&& other, takatori::util::object_creator creator) noexcept
    : quantification(
            other.operator_kind_,
            decltype(operands_) { std::move(other.operands_), creator })
{}

predicate_kind quantification::kind() const noexcept {
    return tag;
}

quantification* quantification::clone(takatori::util::object_creator creator) const& {
    return creator.create_object<quantification>(*this, creator);
}

quantification* quantification::clone(takatori::util::object_creator creator)&& {
    return creator.create_object<quantification>(std::move(*this), creator);
}

quantification::operator_kind_type quantification::operator_kind() const noexcept {
    return operator_kind_;
}

quantification& quantification::operator_kind(operator_kind_type operator_kind) noexcept {
    operator_kind_ = operator_kind;
    return *this;
}

takatori::util::reference_vector<predicate>& quantification::operands() noexcept {
    return operands_;
}

takatori::util::reference_vector<predicate> const& quantification::operands() const noexcept {
    return operands_;
}

bool operator==(quantification const& a, quantification const& b) noexcept {
    return a.operator_kind() == b.operator_kind()
        && a.operands() == b.operands();
}

bool operator!=(quantification const& a, quantification const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, quantification const& value) {
    return out << value.kind() << "("
               << "operator_kind=" << value.operator_kind() << ", "
               << "operands=" << value.operands() << ")";
}

bool quantification::equals(predicate const& other) const noexcept {
    return tag == other.kind() && *this == takatori::util::unsafe_downcast<quantification>(other);
}

std::ostream& quantification::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::variable
