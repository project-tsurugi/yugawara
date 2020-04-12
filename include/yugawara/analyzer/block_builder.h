#pragma once

#include <takatori/graph/graph.h>
#include <takatori/relation/expression.h>
#include <takatori/util/object_creator.h>

#include "block.h"

namespace yugawara::analyzer {

/**
 * @brief builds a graph of basic blocks.
 */
class block_builder {
public:
    /**
     * @brief creates a new empty instance.
     */
    block_builder() = default;

    /**
     * @brief creates a new empty instance.
     * @param creator the object creator
     */
    explicit block_builder(::takatori::util::object_creator creator) noexcept;

    /**
     * @brief creates a new instance.
     * @param blocks the original basic block graph
     */
    explicit block_builder(::takatori::graph::graph<block> blocks) noexcept;

    /**
     * @brief creates a new instance.
     * @param expressions the source relational expression graph
     */
    explicit block_builder(::takatori::graph::graph<::takatori::relation::expression>& expressions);

    /**
     * @brief returns the built graph.
     * @return the built graph
     */
    ::takatori::graph::graph<block>& graph() noexcept;

    /// @copydoc graph()
    ::takatori::graph::graph<block> const& graph() const noexcept;

    /**
     * @brief releases the built graph.
     * @return the built graph
     */
    ::takatori::graph::graph<block> release() noexcept;

    /**
     * @brief returns the object creator.
     * @return the object creator
     */
    ::takatori::util::object_creator get_object_creator() const noexcept;

    /**
     * @brief builds a basic block graph from the given relational expression graph.
     * @param expressions the relational expression graph
     * @return the built graph
     */
    static ::takatori::graph::graph<block> build(
            ::takatori::graph::graph<::takatori::relation::expression>& expressions);

private:
    ::takatori::graph::graph<block> blocks_;
};

} // namespace yugawara::analyzer