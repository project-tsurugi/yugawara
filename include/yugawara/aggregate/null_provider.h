#pragma once

#include "provider.h"

namespace yugawara::aggregate {

/**
 * @brief null implementation of aggregate function declaration provider.
 */
class null_provider : public provider {
public:
    void each(consumer_type const&) const override {}
    void each(std::string_view, std::size_t, consumer_type const&) const override {}
};

} // namespace yugawara::aggregate_function
