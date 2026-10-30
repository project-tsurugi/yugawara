#pragma once

#include <takatori/util/sequence_view.h>

#include <yugawara/storage/index.h>
#include <yugawara/storage/details/search_key_element.h>

#include "details/index_estimator_result.h"

namespace yugawara::analyzer {

/**
 * @brief estimates how many entries the scan/find operation for indices will fetch.
 * @details This only estimates the selectivity and the number of entries.
 *      How to choose an index from the estimations is decided by the compiler
 *      (see index_selection_parameters).
 * @see parametric_index_estimator
 */
class index_estimator {
public:
    /// @brief the scan key piece type.
    using search_key = storage::details::search_key_element;

    /// @brief the estimation result.
    using result = details::index_estimator_result;

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
     * @brief estimates the entries to fetch from the given index.
     * @param index the target index
     * @param search_keys the index key criteria, must be sorted by index key order:
     *      the leading elements are equivalent conditions, and the last one may be a range condition.
     *      This may be empty if the operation scans the whole index
     * @return the estimation result
     */
    [[nodiscard]] virtual result operator()(
            storage::index const& index,
            ::takatori::util::sequence_view<search_key const> search_keys) const = 0;
};

} // namespace yugawara::analyzer
