#pragma once

#include <functional>
#include <unordered_map>

#include <takatori/descriptor/variable.h>
#include <takatori/relation/expression.h>
#include <takatori/util/optional_ptr.h>

#include "stream_variable_flow_info_entry.h"

namespace yugawara::analyzer::details {

/**
 * @brief finds where the stream variables come from, by tracing the relational operators upstream.
 * @details For each input port, this traces the upstream operators while they have just one input,
 *      and collects the stream variables declared by them.
 *      The tracing stops at the first operator which does not have just one input
 *      (e.g. `scan`, `values`, `join`, `union`), or which is regarded as the start of the stream
 *      (`join_find`, `join_scan`, and `escape`).
 *      Such an operator is called the "originator" of the entry.
 *
 *      This only tells which operator declares the variable (the origin):
 *      it does not ensure that the variable is visible at the port
 *      (e.g. a variable declared before `aggregate` is also found after it).
 *
 *      The results are cached for each input port.
 *      If a pass rewrites the graph, it must erase() the entries which trace the changed operators.
 */
class stream_variable_flow_info {
public:
    /// @brief the entry type.
    using entry_type = stream_variable_flow_info_entry;

    /// @brief the size type.
    using size_type = std::size_t;

    /// @brief the consumer type of each().
    using consumer_type = std::function<void(::takatori::descriptor::variable const& defined)>;

    /**
     * @brief creates a new instance.
     */
    stream_variable_flow_info() = default;

    /**
     * @brief returns the entry of the given input port.
     * @details The entry has the variables declared between the port and its originator (inclusive).
     *      This builds the entry if it is not yet cached.
     * @param port the input port
     * @return the entry
     * @throws std::domain_error if the upstream operators are not connected, or are not supported
     */
    [[nodiscard]] entry_type& entry(::takatori::relation::expression::input_port_type const& port);

    /**
     * @brief finds the entry which has the declaration of the given variable.
     * @details If the variable is not found in the entry of the given port,
     *      this continues to the input ports of its originator, unless the originator is `escape`.
     *      `escape` separates the streams, so that the variables are not traced over it.
     * @param variable the target variable
     * @param port the input port where the variable is referred
     * @return the entry which has the declaration, use entry_type::find() to get the declaring operator
     * @return empty if the variable is not found
     * @throws std::domain_error if the upstream operators are not connected, or are not supported
     */
    [[nodiscard]] ::takatori::util::optional_ptr<entry_type> find(
            ::takatori::descriptor::variable const& variable,
            ::takatori::relation::expression::input_port_type const& port);

    /**
     * @brief passes all variables declared in the upstream of the given port.
     * @details This traces the upstream in the same way as find().
     * @param port the input port
     * @param consumer the destination of the declared variables
     * @throws std::domain_error if the upstream operators are not connected, or are not supported
     */
    void each(::takatori::relation::expression::input_port_type const& port, consumer_type const& consumer);

    /**
     * @brief reserves the space of the cached entries.
     * @param size the number of entries
     */
    void reserve(size_type size) {
        entries_.reserve(size);
    }

    /**
     * @brief erases the cached entries of the input ports of the given operator.
     * @param expression the operator
     */
    void erase(::takatori::relation::expression const& expression) {
        for (auto&& port : expression.input_ports()) {
            erase(port);
        }
    }

    /**
     * @brief erases the cached entry of the given input port.
     * @param port the input port
     */
    void erase(::takatori::relation::expression::input_port_type const& port) {
        entries_.erase(std::addressof(port));
    }

    /**
     * @brief erases all cached entries.
     */
    void clear() noexcept {
        entries_.clear();
    }

private:
    // NOTE: use unordered_map to avoid relocation, instead of hopscotch
    std::unordered_map<
            ::takatori::relation::expression::input_port_type const*,
            entry_type,
            std::hash<::takatori::relation::expression::input_port_type const*>,
            std::equal_to<>> entries_;
};

} // namespace yugawara::analyzer::details
