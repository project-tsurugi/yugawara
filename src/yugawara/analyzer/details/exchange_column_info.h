#pragma once

#include <functional>
#include <utility>
#include <vector>

#include <tsl/hopscotch_map.h>
#include <tsl/hopscotch_set.h>

#include <takatori/descriptor/variable.h>
#include <takatori/scalar/expression.h>
#include <takatori/relation/step/offer.h>
#include <takatori/util/optional_ptr.h>
#include <takatori/util/sequence_view.h>

namespace yugawara::analyzer::details {

/**
 * @brief contextual information of rewriting exchange columns.
 */
class exchange_column_info {
public:
    using size_type = std::size_t;

    /**
     * @brief the consumer type which accepts mapping pairs.
     * @param origin the original variable
     * @param exchange_column the destination exchange column
     */
    using mapping_consumer_type = std::function<void(
            ::takatori::descriptor::variable const& origin,
            ::takatori::descriptor::variable const& exchange_column)>;

    [[nodiscard]] ::takatori::util::optional_ptr<::takatori::descriptor::variable const> find(
            ::takatori::descriptor::variable const& variable) const;

    [[nodiscard]] size_type count() const noexcept;

    void register_source(::takatori::relation::step::offer& source);

    [[nodiscard]] std::vector<::takatori::relation::step::offer*> const& sources() const noexcept;

    void register_destination(::takatori::relation::expression& destination);

    [[nodiscard]] std::vector<::takatori::relation::expression*> const& destinations() const noexcept;

    /**
     * @brief enumerate mapping pairs.
     * @param consumer the mapping consumer
     * @see mapping_consumer_type
     */
    void each_mapping(mapping_consumer_type const& consumer) const;

    /**
     * @brief allocates an exchange column for the given stream variable to be exchanged.
     * @param variable the source variable
     * @param force if true, this operation will allocate a new exchange column
     *      even if the given variable has been already allocated.
     * @return the allocated exchange column.
     *      If the given stream variable has been already allocated, this operation will return
     *      the previously allocated column except `force = true`.
     */
    [[nodiscard]] ::takatori::descriptor::variable const& allocate(
            ::takatori::descriptor::variable const& variable,
            bool force = false);

    /**
     * @brief registers stream variable and the corresponded exchange column which is the replacement of former.
     * @param variable the stream variable
     * @param replacement the corresponded exchange column
     */
    void bind(::takatori::descriptor::variable variable, ::takatori::descriptor::variable replacement);

    /**
     * @brief tells that the given exchange column is actually used in some succeeding processes.
     * @param variable the exchange column
     */
    void touch(::takatori::descriptor::variable const& variable);

    /**
     * @brief returns whether or not touch() has been called for the given exchange column.
     * @param variable the exchange column
     * @return true if it was called
     * @return false otherwise
     */
    [[nodiscard]] bool is_touched(::takatori::descriptor::variable const& variable) const;

    /**
     * @brief makes all columns has not been untouched.
     */
    void clear_touched();

    void clear();

private:
    using mapping_type = std::pair<
            ::takatori::descriptor::variable,
            ::takatori::descriptor::variable>;

    std::vector<mapping_type> entries_;

    ::tsl::hopscotch_map<
            ::takatori::descriptor::variable,
            decltype(entries_)::size_type,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>> index_;

    ::tsl::hopscotch_set<
            ::takatori::descriptor::variable,
            std::hash<::takatori::descriptor::variable>,
            std::equal_to<>> touched_;

    std::vector<::takatori::relation::step::offer*> sources_;

    std::vector<::takatori::relation::expression*> destinations_;
};

} // namespace yugawara::analyzer::details
