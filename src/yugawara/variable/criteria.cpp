#include <yugawara/variable/criteria.h>

#include <takatori/util/clonable.h>
#include <takatori/util/downcast.h>

#include <yugawara/variable/comparison.h>

namespace yugawara::variable {

criteria::criteria(class nullity nullity, std::unique_ptr<class predicate> predicate) noexcept
    : nullity_(nullity)
    , predicate_(std::move(predicate))
{}

criteria::criteria(class nullity nullity, class predicate&& predicate)
    : criteria(
            nullity,
            takatori::util::clone_unique(predicate))
{}

static std::unique_ptr<class predicate> wrap_constant(takatori::value::data&& constant) {
    return std::make_unique<comparison>(
            comparison_operator::equal,
            takatori::util::clone_unique(std::move(constant)));
}

criteria::criteria(takatori::value::data&& constant) :
        nullity_ { constant.kind() == ::takatori::value::value_kind::unknown },
        predicate_ { wrap_constant(std::move(constant)) }
{}

criteria::criteria(::takatori::util::clone_tag_t, criteria const& other)
    : criteria(
            other.nullity_,
            takatori::util::clone_unique(other.predicate_))
{}

criteria::criteria(::takatori::util::clone_tag_t, criteria&& other)
    : criteria(
            other.nullity_,
            takatori::util::clone_unique(std::move(other.predicate_)))
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

criteria& criteria::predicate(std::unique_ptr<class predicate> predicate) noexcept {
    predicate_ = std::move(predicate);
    return *this;
}

takatori::util::optional_ptr<takatori::value::data const> criteria::constant() const noexcept {
    return takatori::util::optional_ptr<takatori::value::data const> { shared_constant().get() };
}

std::shared_ptr<takatori::value::data const> const& criteria::shared_constant() const noexcept {
    if (predicate_ && predicate_->kind() == predicate_kind::comparison) {
        auto& cmp = takatori::util::unsafe_downcast<comparison>(*predicate_);
        if (cmp.operator_kind() == comparison_operator::equal) {
            return cmp.shared_value();
        }
    }
    static std::shared_ptr<takatori::value::data const> empty {};
    return empty;
}

bool operator==(criteria const& a, criteria const& b) noexcept {
    return a.nullity() == b.nullity()
        && a.predicate() == b.predicate();
}

bool operator!=(criteria const& a, criteria const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, criteria const& value) {
    return out << "criteria("
               << "nullity=" << value.nullity() << ", "
               << "predicate=" << value.predicate() << ")";
}

} // namespace yugawara::variable
