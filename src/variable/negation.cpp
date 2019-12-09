#include "yugawara/variable/negation.h"

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>

namespace yugawara::variable {

negation::negation(takatori::util::unique_object_ptr<predicate> operand) noexcept
    : operand_(std::move(operand))
{}

negation::negation(predicate&& operand)
    : negation(
            takatori::util::clone_unique(std::move(operand)))
{}

negation::negation(negation const& other, takatori::util::object_creator creator) noexcept
    : negation(
            takatori::util::clone_unique(other.operand_, creator))
{}

negation::negation(negation&& other, takatori::util::object_creator creator) noexcept
    : negation(
            takatori::util::clone_unique(std::move(other.operand_), creator))
{}

predicate_kind negation::kind() const noexcept {
    return tag;
}

negation* negation::clone(takatori::util::object_creator creator) const& {
    return creator.create_object<negation>(*this, creator);
}

negation* negation::clone(takatori::util::object_creator creator)&& {
    return creator.create_object<negation>(std::move(*this), creator);
}

predicate& negation::operand() noexcept {
    return *operand_;
}

predicate const& negation::operand() const noexcept {
    return *operand_;
}

takatori::util::optional_ptr<predicate> negation::optional_operand() noexcept {
    return takatori::util::optional_ptr { operand_.get() };
}

takatori::util::optional_ptr<predicate const> negation::optional_operand() const noexcept {
    return takatori::util::optional_ptr { operand_.get() };
}

takatori::util::unique_object_ptr<predicate> negation::release_operand() noexcept {
    return std::move(operand_);
}

negation& negation::operand(takatori::util::unique_object_ptr<predicate> operand) noexcept {
    operand_ = std::move(operand);
    return *this;
}

bool operator==(negation const& a, negation const& b) noexcept {
    return a.optional_operand() == b.optional_operand();
}

bool operator!=(negation const& a, negation const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, negation const& value) {
    return out << value.kind() << "("
               << "operand=" << value.optional_operand() << ")";
}

bool negation::equals(predicate const& other) const noexcept {
    return tag == other.kind() && *this == takatori::util::unsafe_downcast<negation>(other);
}

std::ostream& negation::print_to(std::ostream& out) const {
    return out << *this;
}

} // namespace yugawara::variable
