#pragma once

#include <functional>

#include <takatori/util/sequence_view.h>

#include <yugawara/storage/index.h>
#include <yugawara/storage/details/search_key_element.h>

#include "details/index_estimator_result.h"

namespace yugawara::analyzer {

/**
 * @brief estimates cost of scan/find operation for indices.
 * @see basic_estimator
 */
class index_estimator {
public:
    /// @brief the scan key piece type.
    using search_key = storage::details::search_key_element;

    /// @brief the sort key piece type.
    using sort_key = storage::details::index_key_element;

    /// @brief the column reference type.
    using column_ref = std::reference_wrapper<storage::column const>;

    /// @brief the estimation result.
    using result = details::index_estimator_result;

    /// @brief the index access attribute.
    using attribute = details::index_estimator_result_attribute;

    /**
     * @brief creates a new instance.
     */
    constexpr index_estimator() = default;

    /**
     * @brief destroys this instance.
     */
    virtual ~index_estimator() = default;

    /**
     * @brief creates a new instance.
     * @param other the copy source
     */
    index_estimator(index_estimator const& other) = default;

    /**
     * @brief assigns the given object into this.
     * @param other the copy source
     * @return this
     */
    index_estimator& operator=(index_estimator const& other) = default;

    /**
     * @brief creates a new instance.
     * @param other the move source
     */
    index_estimator(index_estimator&& other) noexcept = default;

    /**
     * @brief assigns the given object into this.
     * @param other the move source
     * @return this
     */
    index_estimator& operator=(index_estimator&& other) noexcept = default;

    /**
     * @brief estimates the operation cost of the given index.
     * @param index the target index
     * @param search_keys the index key criteria, must be sorted by index key order
     * @param sort_keys the requested index order, may not match with the index
     * @param values the all columns to obtain from/via the index, may include index keys
     * @return the estimation result
     */
    [[nodiscard]] virtual result operator()(
            storage::index const& index,
            ::takatori::util::sequence_view<search_key const> search_keys,
            ::takatori::util::sequence_view<sort_key const> sort_keys,
            ::takatori::util::sequence_view<column_ref const> values) const = 0;
};

} // namespace yugawara::analyzer