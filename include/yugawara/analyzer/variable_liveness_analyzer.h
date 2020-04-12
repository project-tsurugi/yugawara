#pragma once

#include <unordered_map>

#include <takatori/graph/graph.h>

#include <takatori/descriptor/variable.h>

#include <takatori/util/enum_set.h>
#include <takatori/util/object_creator.h>

#include "variable_liveness_kind.h"
#include "variable_liveness_info.h"
#include "block.h"

namespace yugawara::analyzer {

/**
 * @brief analyzes liveness of variables in each block.
 * @details This is designed for relational operator graphs of "step plan", that is, the graph must not contain
 *      expressions which only can appear "intermediate plan".
 * @see block
 * @see block_builder
 * @see variable_liveness_info
 */
class variable_liveness_analyzer {
public:
    /// @brief the liveness information kind.
    using kind_type = variable_liveness_kind;

    /// @brief set type of kind_type.
    using kind_set = variable_liveness_kind_set;

    /// @brief liveness information.
    using info = variable_liveness_info;

    /// @brief liveness information map type.
    using info_map = std::unordered_map<
            block const*,
            info,
            ::std::hash<block const*>,
            ::std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<block const* const, info>>>;

    /**
     * @brief creates a new instance.
     * @param blocks the analysis target blocks, which must be built by block_builder
     * @attention the specified graph must not modify nor be disposed while analyzing
     */
    explicit variable_liveness_analyzer(::takatori::graph::graph<block> const& blocks);

    /**
     * @brief creates a new instance.
     * @param blocks the analysis target blocks, which must be built by block_builder
     * @param creator the object creator
     * @attention the specified graph must not modify nor be disposed while analyzing
     */
    explicit variable_liveness_analyzer(
            ::takatori::graph::graph<block> const& blocks,
            ::takatori::util::object_creator creator);

    /**
     * @brief inspects the given block and returns its liveness information.
     * @param target the target block
     * @param require the required information kind
     * @return the liveness information of the block, including the specified liveness information
     * @throws std::invalid_argument if the specified block is out of the scope of this analyzer,
     *      or some block contains semantic error
     */
    info& inspect(
            block const& target,
            kind_set require = {
                    kind_type::define,
                    kind_type::use,
                    kind_type::kill,
            });

private:
    info_map blocks_ {};
    kind_set resolved_ {};
};

} // namespace yugawara::analyzer
