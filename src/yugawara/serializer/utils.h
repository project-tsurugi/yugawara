#pragma once

#include <ostream>

#include <takatori/relation/graph.h>

#include <yugawara/analyzer/expression_mapping.h>
#include <yugawara/analyzer/variable_mapping.h>

namespace yugawara::serializer {

/**
 * @brief dump a relation expression graph.
 * @param output the target output stream
 * @param graph the relation expression graph to dump
 * @param variable_mapping the mapping from variables to their string representation
 * @param expression_mapping the mapping from relation expressions to their string representation
 */
std::ostream& dump(
        std::ostream& output,
        ::takatori::relation::graph_type const& graph,
        analyzer::variable_mapping const& variable_mapping = {},
        analyzer::expression_mapping const& expression_mapping = {});

/**
 * @brief dump a relation expression graph to standard output.
 * @param graph the relation expression graph to dump
 * @param variable_mapping the mapping from variables to their string representation
 * @param expression_mapping the mapping from relation expressions to their string representation
 */
std::ostream& dump(
        ::takatori::relation::graph_type const& graph,
        analyzer::variable_mapping const& variable_mapping = {},
        analyzer::expression_mapping const& expression_mapping = {});

} // namespace yugawara::serializer
