#pragma once

#include "provider.h"

namespace yugawara::type {

/**
 * @brief null implementation of type declaration provider.
 */
class null_provider : public provider {
public:
    void each(consumer_type const&) const override {}
    [[nodiscard]] std::shared_ptr<declaration const> find(std::string_view) const override {
        return {};
    }
};

} // namespace yugawara::variable_function
