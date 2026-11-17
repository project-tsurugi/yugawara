#pragma once

#include <unordered_map>

#include <takatori/relation/intermediate/join.h>
#include <takatori/relation/intermediate/aggregate.h>

#include <takatori/util/optional_ptr.h>

#include <yugawara/runtime_feature.h>
#include <yugawara/analyzer/join_info.h>
#include <yugawara/analyzer/aggregate_info.h>

namespace yugawara::analyzer::details {

/**
 * @brief represents step execution planning information.
 */
class step_plan_builder_options {
public:
    /// @brief the join hint map type.
    using join_hint_map = std::unordered_map<
            ::takatori::relation::intermediate::join const*,
            join_info,
            std::hash<::takatori::relation::intermediate::join const*>,
            std::equal_to<>>;

    /// @brief the aggregation hint map type.
    using aggregate_hint_map = std::unordered_map<
            ::takatori::relation::intermediate::aggregate const*,
            aggregate_info,
            std::hash<::takatori::relation::intermediate::aggregate const*>,
            std::equal_to<>>;

    /**
     * @brief returns the available feature set of the target environment.
     * @return the available features
     */
    [[nodiscard]] runtime_feature_set& runtime_features() noexcept;

    /// @copydoc runtime_features()
    [[nodiscard]] runtime_feature_set const& runtime_features() const noexcept;

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

private:
    join_hint_map join_hints_;
    aggregate_hint_map aggregate_hints_;
    runtime_feature_set runtime_features_ { runtime_feature_all };
};

} // namespace yugawara::analyzer::details
