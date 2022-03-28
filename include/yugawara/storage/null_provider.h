#pragma once

#include "provider.h"

namespace yugawara::storage {

/**
 * @brief null implementation of provider.
 */
class null_provider : public provider {
public:
    [[nodiscard]] std::shared_ptr<relation const> find_relation(std::string_view) const override {
        return {};
    }
    [[nodiscard]] std::shared_ptr<index const> find_index(std::string_view) const override {
        return {};
    }
    [[nodiscard]] std::shared_ptr<sequence const> find_sequence(std::string_view) const override {
        return {};
    }
    void each_relation(relation_consumer_type) const override {}
    void each_index(index_consumer_type const&) const override {}
    void each_sequence(sequence_consumer_type const&) const override {}
};

} // namespace yugawara::storage
