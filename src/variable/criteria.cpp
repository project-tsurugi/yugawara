#include "yugawara/variable/criteria.h"

#include <takatori/util/clonable.h>

namespace yugawara::variable {

criteria::criteria(class nullity nullity, takatori::util::unique_object_ptr<class predicate> predicate) noexcept
    : nullity_(nullity)
    , predicate_(std::move(predicate))
{}

criteria::criteria(class nullity nullity, class predicate&& predicate)
    : criteria(
            nullity,
            takatori::util::clone_unique(predicate))
{}

criteria::criteria(criteria const& other, takatori::util::object_creator creator)
    : criteria(
            other.nullity_,
            takatori::util::clone_unique(other.predicate_, creator))
{}

criteria::criteria(criteria&& other, takatori::util::object_creator creator)
    : criteria(
            other.nullity_,
            takatori::util::clone_unique(std::move(other.predicate_), creator))
{}

class nullity criteria::nullity() const noexcept {
    return nullity_;
}

criteria& criteria::nullity(class nullity nullity) noexcept {
    nullity_ = nullity;
    return *this;
}

takatori::util::optional_ptr<class predicate const> criteria::predicate() const noexcept {
    return takatori::util::optional_ptr<class predicate const> { predicate_.get() };
}

criteria& criteria::predicate(takatori::util::unique_object_ptr<class predicate> predicate) noexcept {
    predicate_ = std::move(predicate);
    return *this;
}

std::ostream& operator<<(std::ostream& out, criteria const& value) {
    return out << "criteria("
               << "nullity=" << value.nullity() << ", "
               << "predicate=" << value.predicate() << ")";
}

} // namespace yugawara::variable
