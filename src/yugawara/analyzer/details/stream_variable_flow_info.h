#pragma once

#include <functional>
#include <unordered_map>

#include <takatori/descriptor/variable.h>
#include <takatori/relation/expression.h>
#include <takatori/util/optional_ptr.h>

#include "stream_variable_flow_info_entry.h"

namespace yugawara::analyzer::details {

class stream_variable_flow_info {
public:
    using entry_type = stream_variable_flow_info_entry;
    using size_type = std::size_t;
    using consumer_type = std::function<void(::takatori::descriptor::variable const& defined)>;

    stream_variable_flow_info() = default;

    [[nodiscard]] entry_type& entry(::takatori::relation::expression::input_port_type const& port);

    [[nodiscard]] ::takatori::util::optional_ptr<entry_type> find(
            ::takatori::descriptor::variable const& variable,
            ::takatori::relation::expression::input_port_type const& port);

    void each(::takatori::relation::expression::input_port_type const& port, consumer_type const& consumer);

    void reserve(size_type size) {
        entries_.reserve(size);
    }

    void erase(::takatori::relation::expression const& expression) {
        for (auto&& port : expression.input_ports()) {
            erase(port);
        }
    }

    void erase(::takatori::relation::expression::input_port_type const& port) {
        entries_.erase(std::addressof(port));
    }

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
