#pragma once

#include <takatori/util/optional_ptr.h>

namespace yugawara::extension {

template<class TPort, class TGraph>
static ::takatori::util::optional_ptr<TPort> find_output_port_internal(TGraph& graph) noexcept {
    using output_port_type = TPort;
    ::takatori::util::optional_ptr<output_port_type> result {};
    for (auto&& expr : graph) {
        for (auto&& output : expr.output_ports()) {
            if (output.opposite() == nullptr) {
                if (result) {
                    // ambiguous
                    return {};
                }
                result.reset(output);
            }
        }
    }
    return result;
}

} // namespace yugawara::extension
