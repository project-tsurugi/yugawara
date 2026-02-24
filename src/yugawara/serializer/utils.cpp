#include "utils.h"

#include <iostream>

#include <takatori/serializer/json_printer.h>
#include <yugawara/serializer/object_scanner.h>

namespace yugawara::serializer {

std::ostream& dump(
        std::ostream& output,
        ::takatori::relation::graph_type const& graph,
        analyzer::variable_mapping const& variable_mapping,
        analyzer::expression_mapping const& expression_mapping) {
    ::takatori::serializer::json_printer printer { output };
    ::yugawara::serializer::object_scanner scanner {
            std::make_shared<analyzer::variable_mapping>(variable_mapping),
            std::make_shared<analyzer::expression_mapping>(expression_mapping),
    };
    scanner(graph, printer);
    return output;
}

std::ostream& dump(
        ::takatori::relation::graph_type const& graph,
        analyzer::variable_mapping const& variable_mapping,
        analyzer::expression_mapping const& expression_mapping) {
    return dump(std::cout, graph, variable_mapping, expression_mapping);
}

} // namespace yugawara::serializer
