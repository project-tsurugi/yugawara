#pragma once

#include <optional>

#include <tsl/hopscotch_map.h>

#include <takatori/relation/expression.h>

namespace yugawara::analyzer::details {

/**
 * @brief provides the simulated data size of individual relational operator output.
 */
class flow_volume_info {
public:
    using input_type = ::takatori::relation::expression::input_port_type;
    using output_type = ::takatori::relation::expression::output_port_type;

    struct volume_info {
        double row_count;
        double column_size;
    };

    explicit flow_volume_info() = default;

    [[nodiscard]] std::optional<volume_info> find(input_type const& port) const {
        if (port.opposite()) {
            return find(*port.opposite());
        }
        return {};
    }

    [[nodiscard]] std::optional<volume_info> find(output_type const& port) const {
        if (auto it = entries_.find(std::addressof(port)); it != entries_.end()) {
            return it->second;
        }
        return {};
    }

    flow_volume_info& add(output_type const& port, volume_info info) {
        entries_.insert_or_assign(std::addressof(port), info);
        return *this;
    }

private:
    ::tsl::hopscotch_map<
            output_type const*, // NOTE: use output port: input will be relocated during join optimization
            volume_info,
            std::hash<output_type const*>,
            std::equal_to<>> entries_;
};

} // namespace yugawara::analyzer::details
