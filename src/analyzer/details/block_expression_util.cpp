#include <yugawara/analyzer/details/block_expression_util.h>

namespace yugawara::analyzer::details {

::takatori::util::optional_ptr<::takatori::relation::expression>
upstream_unambiguous(::takatori::relation::expression& element) noexcept {
    auto ports = element.input_ports();
    if (ports.size() != 1) {
        return {};
    }
    auto opposite = ports.front().opposite();
    if (!opposite) {
        return {};
    }
    return opposite.value().owner();
}

::takatori::util::optional_ptr<::takatori::relation::expression const>
upstream_unambiguous(::takatori::relation::expression const& element) noexcept {
    auto ports = element.input_ports();
    if (ports.size() != 1) {
        return {};
    }
    auto opposite = ports.front().opposite();
    if (!opposite) {
        return {};
    }
    return opposite.value().owner();
}

::takatori::util::optional_ptr<::takatori::relation::expression>
downstream_unambiguous(::takatori::relation::expression& element) noexcept {
    auto ports = element.output_ports();
    if (ports.size() != 1) {
        return {};
    }
    auto opposite = ports.front().opposite();
    if (!opposite) {
        return {};
    }
    return opposite.value().owner();
}

::takatori::util::optional_ptr<::takatori::relation::expression const>
downstream_unambiguous(::takatori::relation::expression const& element) noexcept {
    auto ports = element.output_ports();
    if (ports.size() != 1) {
        return {};
    }
    auto opposite = ports.front().opposite();
    if (!opposite) {
        return {};
    }
    return opposite.value().owner();
}

} // namespace yugawara::analyzer::details
