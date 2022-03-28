#pragma once

#include "provider.h"

namespace yugawara::function {

/**
 * @brief null implementation of function declaration provider.
 */
class null_provider : public provider {
public:
    void each(consumer_type const&) const override {}
    void each(std::string_view, std::size_t, consumer_type const&) const override {}
};

} // namespace yugawara::function
