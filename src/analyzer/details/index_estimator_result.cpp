#include <yugawara/analyzer/details/index_estimator_result.h>

#include <takatori/util/optional_print_support.h>

namespace yugawara::analyzer::details {

index_estimator_result::index_estimator_result(
        score_type score,
        std::optional<size_type> count,
        attribute_set attributes) noexcept
    : score_(score)
    , count_(std::move(count))
    , attributes_(attributes)
{}

index_estimator_result::score_type index_estimator_result::score() const noexcept {
    return score_;
}

std::optional<index_estimator_result::size_type> index_estimator_result::count() const noexcept {
    return count_;
}

index_estimator_result::attribute_set index_estimator_result::attributes() const noexcept {
    return attributes_;
}

std::ostream& operator<<(std::ostream& out, index_estimator_result const& value) {
    using ::takatori::util::print_support;
    return out << "estimation_result("
               << "score=" << value.score() << ", "
               << "count=" << print_support { value.count() } << ", "
               << "attributes=" << value.attributes() << ")";
}

} // namespace yugawara::analyzer::details
