#pragma once

#include <ostream>
#include <optional>
#include <utility>

#include <cstddef>

#include "index_estimator_result_attribute.h"

namespace yugawara::analyzer::details {

/**
 * @brief estimates cost of scan/find operation for indices.
 */
class index_estimator_result {
public:
    /// @brief the score type.
    using score_type = double;

    /// @brief the size type.
    using size_type = std::size_t;

    /// @brief the result attribute type.
    using attribute = index_estimator_result_attribute;

    /// @brief the result attribute set type.
    using attribute_set = index_estimator_result_attribute_set ;

    /**
     * @brief creates a new empty instance.
     */
    index_estimator_result() = default;

    /**
     * @brief creates a new instance.
     * @param score the estimation score
     * @param count the estimated entry count
     * @param attributes the access attributes
     */
    index_estimator_result(
            score_type score,
            std::optional<size_type> count,
            attribute_set attributes) noexcept;

    /**
     * @brief returns the estimation score.
     * @return the estimation score (higher is better)
     */
    [[nodiscard]] score_type score() const noexcept;

    /**
     * @brief returns the estimated number of entries to fetch.
     * @return the estimated number of entries
     */
    [[nodiscard]] std::optional<size_type> count() const noexcept;

    /**
     * @brief returns whether or not the operation can only access the index.
     * @return true if fetches only from the index
     * @return false if fetches table via the index
     */
    [[nodiscard]] attribute_set attributes() const noexcept;

private:
    score_type score_ { 0 };
    std::optional<size_type> count_ {};
    attribute_set attributes_ {};
};

/**
 * @brief appends string representation of the given value.
 * @param out the target output
 * @param value the target value
 * @return the output
 */
std::ostream& operator<<(std::ostream& out, index_estimator_result const& value);

} // namespace yugawara::analyzer::details