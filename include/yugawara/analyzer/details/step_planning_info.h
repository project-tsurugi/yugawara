#pragma once

#include <unordered_map>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>

#include <takatori/util/optional_ptr.h>

#include "../join_info.h"
#include "../aggregate_info.h"

namespace yugawara::analyzer::details {

/**
 * @brief represents step execution planning information.
 */
class step_planning_info {
public:
    /// @brief the join hint map type.
    using join_hint_map = std::unordered_map<
            ::takatori::relation::intermediate::join const*,
            join_info,
            std::hash<::takatori::relation::intermediate::join const*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::relation::intermediate::join const* const,
                    join_info>>>;

    /// @brief the aggregation hint map type.
    using aggregate_hint_map = std::unordered_map<
            ::takatori::relation::intermediate::aggregate const*,
            aggregate_info,
            std::hash<::takatori::relation::intermediate::aggregate const*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::relation::intermediate::aggregate const* const,
                    aggregate_info>>>;

    /**
     * @brief creates a new instance.
     * @param creator the object creator
     */
    explicit step_planning_info(::takatori::util::object_creator creator = {}) noexcept;

    /**
     * @brief registers hint for the given join operation.
     * @param expr the join operation
     * @param info the hint information
     * @throws std::invalid_argument if the target expression has been already registered
     */
    void add(::takatori::relation::intermediate::join const& expr, join_info info);

    /**
     * @brief returns the hint of the join operation.
     * @param expr the target join operation
     * @return the registered hint
     * @return empty there are no registered hint for the operation
     */
    ::takatori::util::optional_ptr<join_info const> find(::takatori::relation::intermediate::join const& expr) const;

    /**
     * @brief registers hint for the given aggregate operation.
     * @param expr the aggregate operation
     * @param info the hint information
     * @throws std::invalid_argument if the target expression has been already registered
     */
    void add(::takatori::relation::intermediate::aggregate const& expr, aggregate_info info);

    /**
     * @brief returns the hint of the aggregate operation.
     * @param expr the target aggregate operation
     * @return the registered hint
     * @return empty there are no registered hint for the operation
     */
    ::takatori::util::optional_ptr<aggregate_info const> find(::takatori::relation::intermediate::aggregate const& expr) const;

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept;

private:
    join_hint_map join_hints_;
    aggregate_hint_map aggregate_hints_;
};

} // namespace yugawara::analyzer::details
