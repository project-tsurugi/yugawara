#pragma once

#include <unordered_map>

#include <takatori/relation/expression.h>
#include <takatori/util/optional_ptr.h>

#include "stream_variable_flow_info_entry.h"

namespace yugawara::analyzer::details {

class stream_variable_flow_info {
public:
    using entry_type = stream_variable_flow_info_entry;

    using size_type = std::size_t;

    stream_variable_flow_info() = default;

    explicit stream_variable_flow_info(::takatori::util::object_creator creator)
        : entries_(creator.allocator())
    {}

    [[nodiscard]] ::takatori::util::optional_ptr<entry_type> find(
            ::takatori::descriptor::variable const& variable,
            ::takatori::relation::expression::input_port_type const& port);

    [[nodiscard]] entry_type& entry(::takatori::relation::expression::input_port_type const& port);

    void reserve(size_type size) {
        entries_.reserve(size);
    }

    void clear() noexcept {
        entries_.clear();
    }

    [[nodiscard]] ::takatori::util::object_creator get_object_creator() const {
        return entries_.get_allocator().resource();
    }

private:
    // NOTE: use unordered_map to avoid relocation, instead of hopscotch
    std::unordered_map<
            ::takatori::relation::expression::input_port_type const*,
            entry_type,
            std::hash<::takatori::relation::expression::input_port_type const*>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<
                    ::takatori::relation::expression::input_port_type const* const,
                    entry_type>>> entries_;
};

} // namespace yugawara::analyzer::details
