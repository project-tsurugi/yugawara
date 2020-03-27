#pragma once

#include <functional>

#include <takatori/graph/graph.h>
#include <takatori/util/optional_ptr.h>

#include "block.h"

namespace yugawara::analyzer {

/// @brief graph of block.
using block_graph = ::takatori::graph::graph<block>;

/// @brief block consumer.
using block_consumer = std::function<void(block&)>;

/// @brief block consumer.
using const_block_consumer = std::function<void(block const&)>;

/**
 * @brief collects the blocks which do not have any upstream blocks.
 * @param g the target graph
 * @param consumer the result consumer
 */
void collect_heads(block_graph& g, block_consumer const& consumer);

/// @copydoc collect_heads()
void collect_heads(block_graph const& g, const_block_consumer const& consumer);

/**
 * @brief collects the blocks which do not have any downstream blocks.
 * @param g the target graph
 * @param consumer the result consumer
 */
void collect_tails(block_graph& g, block_consumer const& consumer);

/// @copydoc collect_tails()
void collect_tails(block_graph const& g, const_block_consumer const& consumer);

/**
 * @brief returns a block which do not have any upstream blocks.
 * @param g the target graph
 * @return the head block
 * @return empty if there are no blocks, or are two or more head blocks
 */
::takatori::util::optional_ptr<block> find_unique_head(block_graph& g) noexcept;

/// @copydoc find_unique_head()
::takatori::util::optional_ptr<block const> find_unique_head(block_graph const& g) noexcept;

/**
 * @brief returns the upstream block of the given block.
 * @param b the target block
 * @return the succeeding block
 * @return empty if there are no upstream blocks, or are two or more
 */
::takatori::util::optional_ptr<block> find_unique_upstream(block& b) noexcept;

/// @copydoc find_unique_upstream()
::takatori::util::optional_ptr<block const> find_unique_upstream(block const& b) noexcept;

/**
 * @brief returns the downstream block of the given block.
 * @param b the target block
 * @return the succeeding block
 * @return empty if there are no downstream blocks, or are two or more
 */
::takatori::util::optional_ptr<block> find_unique_downstream(block& b) noexcept;

/// @copydoc find_unique_downstream()
::takatori::util::optional_ptr<block const> find_unique_downstream(block const& b) noexcept;

} // namespace yugawara::analyzer
