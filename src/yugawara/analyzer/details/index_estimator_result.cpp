#include <yugawara/analyzer/details/index_estimator_result.h>

#include <takatori/util/optional_print_support.h>

namespace yugawara::analyzer::details {

index_estimator_result::index_estimator_result(
        selectivity_type selectivity,
        std::optional<size_type> count) noexcept
    : selectivity_(selectivity)
    , count_(count)
{}

index_estimator_result::selectivity_type index_estimator_result::selectivity() const noexcept {
    return selectivity_;
}

std::optional<index_estimator_result::size_type> index_estimator_result::count() const noexcept {
    return count_;
}

std::ostream& operator<<(std::ostream& out, index_estimator_result const& value) {
    using ::takatori::util::print_support;
    return out << "estimation_result("
               << "selectivity=" << value.selectivity() << ", "
               << "count=" << print_support { value.count() } << ")";
}

} // namespace yugawara::analyzer::details
