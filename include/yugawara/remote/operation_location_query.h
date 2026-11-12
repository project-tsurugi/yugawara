#pragma once

#include <takatori/plan/step.h>
#include <takatori/statement/statement.h>

#include "location_info.h"

namespace yugawara::remote {

/**
 * @brief an abstract interface to query where each operation can be executed.
 * @details The compiler provides its implementation as a part of the compilation result.
 *      It accepts only the operations in the compiled statement.
 */
class operation_location_query {
public:
    /// @brief numeric location ID type (available until database restart).
    using location_id_type = location_info::location_id_type;

    /// @brief set of numeric location IDs type.
    using location_id_set_type = location_info::location_id_set_type;

    /**
     * @brief creates a new instance.
     */
    constexpr operation_location_query() = default;

    /**
     * @brief destroys this instance.
     */
    virtual ~operation_location_query() = default;

    operation_location_query(operation_location_query const& other) = delete;
    operation_location_query& operator=(operation_location_query const& other) = delete;
    operation_location_query(operation_location_query&& other) noexcept = delete;
    operation_location_query& operator=(operation_location_query&& other) noexcept = delete;

    /**
     * @brief returns the set of location IDs where the given step can be executed.
     * @param operation the step (process or exchange) to query
     * @return the set of location IDs
     */
    [[nodiscard]] virtual location_id_set_type find(::takatori::plan::step const& operation) const = 0;

    /**
     * @brief returns the set of location IDs where the given statement can be executed.
     * @details If the statement consists of two or more steps,
     *      the result is the intersection of the location IDs of the individual steps.
     *      Thus, it may be empty if the statement includes remote execution.
     * @param operation the statement to query
     * @return the set of location IDs
     */
    [[nodiscard]] virtual location_id_set_type find(::takatori::statement::statement const& operation) const = 0;
};

} // namespace yugawara::remote
