#pragma once

#include <ostream>
#include <optional>

#include <cstddef>

namespace yugawara::analyzer::details {

/**
 * @brief the estimation result of index access.
 */
class index_estimator_result {
public:
    /// @brief the selectivity type.
    using selectivity_type = double;

    /// @brief the size type.
    using size_type = std::size_t;

    /**
     * @brief creates a new instance, which represents the whole index entries.
     */
    index_estimator_result() = default;

    /**
     * @brief creates a new instance.
     * @param selectivity the estimated selectivity, the ratio of entries to fetch in the whole index
     * @param count the estimated number of entries to fetch, or empty if it is unknown
     */
    explicit index_estimator_result(
            selectivity_type selectivity,
            std::optional<size_type> count = {}) noexcept;

    /**
     * @brief returns the estimated selectivity.
     * @return the ratio of entries to fetch in the whole index, in the range of `[0, 1]` (lower is better)
     */
    [[nodiscard]] selectivity_type selectivity() const noexcept;

    /**
     * @brief returns the estimated number of entries to fetch.
     * @return the estimated number of entries
     * @return empty if it is unknown
     */
    [[nodiscard]] std::optional<size_type> count() const noexcept;

private:
    selectivity_type selectivity_ { 1.0 };
    std::optional<size_type> count_ {};
};

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
std::ostream& operator<<(std::ostream& out, index_estimator_result const& value);

} // namespace yugawara::analyzer::details
