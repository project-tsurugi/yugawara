#pragma once

#include <vector>

#include <tsl/hopscotch_map.h>

#include <takatori/descriptor/variable.h>

#include <takatori/relation/scan.h>

#include <takatori/util/object_creator.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/ownership_reference.h>
#include <takatori/util/sequence_view.h>

#include <yugawara/storage/table.h>
#include <yugawara/storage/index.h>

#include "search_key_term_builder.h"

namespace yugawara::analyzer::details {

/**
 * @brief collects scan keys.
 */
class scan_key_collector {
public:
    /// @brief the term of individual scan key elements.
    using term = search_key_term;

    /// @brief the sort key piece type.
    using sort_key = storage::details::index_key_element;

    /// @brief the column reference type.
    using column_ref = std::reference_wrapper<storage::column const>;

    explicit scan_key_collector(::takatori::util::object_creator creator);

    /**
     * @brief collect scan key terms for the target scan expression.
     * @details The target scan operation must not have any endpoint settings.
     *
     *      This also collect from the succeeding `join_relation` operation it it is required.
     *      If there are one or more filters between `scan` and `join_relation`,
     *
     *      - If it includes anti join, must not use this function
     *      - Otherwise, merge all filter conditions except scan key into its join condition manually
     *
     *      Note that, this don't care the `scan` reaches which input port of the succeeding `join_relation`.
     *
     *      If `include_join` is enabled but there is no succeeding `join_relation`, this will return `false`.
     * @param expression the target expression
     * @param include_join whether or not consider the succeeding join conditions
     * @return true if any scan key is available
     * @return false otherwise
     */
    [[nodiscard]] bool operator()(::takatori::relation::scan& expression, bool include_join = false);

    void clear();

    [[nodiscard]] storage::table const& table() const;

    [[nodiscard]] ::takatori::util::optional_ptr<term> find(storage::column const& column);

    [[nodiscard]] ::takatori::util::sequence_view<sort_key const> sort_keys() const;

    [[nodiscard]] ::takatori::util::sequence_view<column_ref const> referring() const;

    /**
     * @brief returns the object creator to create/delete objects in this container.
     * @return the object creator for this container
     */
    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const noexcept;

private:
    ::takatori::util::object_creator creator_;

    search_key_term_builder term_builder_;
    ::tsl::hopscotch_map<
            storage::column const*,
            ::takatori::descriptor::variable,
            std::hash<storage::column const*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    storage::column const* const,
                    ::takatori::descriptor::variable>>> column_map_;

    ::takatori::util::optional_ptr<storage::table const> table_ {};
    std::vector<sort_key, ::takatori::util::object_allocator<sort_key>> sort_keys_;
    std::vector<column_ref, ::takatori::util::object_allocator<column_ref>> referring_;
};

} // namespace yugawara::analyzer::details