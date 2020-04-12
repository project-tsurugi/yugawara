#include <yugawara/analyzer/join_info.h>

namespace yugawara::analyzer {

join_info::join_info(strategy_type strategy) noexcept
    : strategy_(strategy)
{}

join_info::strategy_type join_info::strategy() const noexcept {
    return strategy_;
}

join_info& join_info::strategy(join_info::strategy_type strategy) noexcept {
    strategy_ = strategy;
    return *this;
}

std::ostream& operator<<(std::ostream& out, join_info const& value) {
    return out << "join_info("
            << "strategy=" << value.strategy() << ")";
}

} // namespace yugawara::analyzer
