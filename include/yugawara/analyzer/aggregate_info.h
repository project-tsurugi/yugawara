#pragma once

#include <iostream>

namespace yugawara::analyzer {

/**
 * @brief a planning information about aggregate operations.
 * @see takatori::relation::intermediate::aggregate
 */
class aggregate_info {
public:
    /// @brief the default value of whether or not incremental aggregation is enabled.
    static constexpr bool default_incremental = false;

    /**
     * @brief creates a new instance which has default settings.
     */
    aggregate_info() = default;

    /**
     * @brief creates a new instance.
     * @param incremental whether or not incremental aggregation is enabled
     */
    explicit aggregate_info(bool incremental) noexcept;

    /**
     * @brief returns whether or not incremental aggregation is enabled
     * @return true if the target operation enables incremental aggregation
     * @return false otherwise
     */
    bool incremental() const noexcept;

    /**
     * @brief sets whether or not incremental aggregation is enabled.
     * @param enabled whether or not incremental aggregation is enabled
     * @return this
     */
    aggregate_info& incremental(bool enabled) noexcept;

private:
    bool incremental_ { default_incremental };
};

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output stream
 */
std::ostream& operator<<(std::ostream& out, aggregate_info const& value);

} // namespace yugawara::analyzer
