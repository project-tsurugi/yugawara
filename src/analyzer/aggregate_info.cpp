#include <yugawara/analyzer/aggregate_info.h>

namespace yugawara::analyzer {

aggregate_info::aggregate_info(strategy_type strategy) noexcept
    : strategy_(strategy)
{}

aggregate_info::strategy_type aggregate_info::strategy() const noexcept {
    return strategy_;
}

aggregate_info& aggregate_info::strategy(aggregate_info::strategy_type strategy) noexcept {
    strategy_ = strategy;
    return *this;
}

std::ostream& operator<<(std::ostream& out, aggregate_info const& value) {
    return out << "aggregate_info("
               << "strategy=" << value.strategy() << ")";
}

} // namespace yugawara::analyzer
