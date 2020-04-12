#include <yugawara/analyzer/aggregate_info.h>

namespace yugawara::analyzer {

aggregate_info::aggregate_info(bool incremental) noexcept
    : incremental_(incremental)
{}

bool aggregate_info::incremental() const noexcept {
    return incremental_;
}

aggregate_info& aggregate_info::incremental(bool enabled) noexcept {
    incremental_ = enabled;
    return *this;
}

std::ostream& operator<<(std::ostream& out, aggregate_info const& value) {
    return out << "aggregate_info("
               << "incremental=" << (value.incremental() ? "true" : "false") << ")";
}

} // namespace yugawara::analyzer
