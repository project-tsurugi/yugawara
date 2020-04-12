#pragma once

#include <unordered_map>

#include <takatori/descriptor/relation.h>
#include <takatori/plan/exchange.h>

#include "exchange_column_info.h"

namespace yugawara::analyzer::details {

/**
 * @brief contextual information of rewriting exchange columns.
 */
class exchange_column_info_map {
public:
    using size_type = std::size_t;
    using key_type = ::takatori::plan::exchange const*;
    using element_type = exchange_column_info;

    explicit exchange_column_info_map(::takatori::util::object_creator creator) noexcept;

    bool empty() const noexcept;

    size_type size() const noexcept;

    element_type& get(::takatori::plan::exchange const& declaration);

    element_type& get(::takatori::descriptor::relation const& relation);

    element_type& create_or_get(::takatori::descriptor::relation const& relation);

    void erase(::takatori::descriptor::relation const& relation);

    static bool is_target(::takatori::descriptor::relation const& relation);

    ::takatori::util::object_creator get_object_creator() const {
        return mappings_.get_allocator().resource();
    }

private:
    std::unordered_map<
            key_type,
            element_type,
            std::hash<key_type>,
            std::equal_to<>,
            ::takatori::util::object_allocator<std::pair<key_type const, element_type>>> mappings_;
};

} // namespace yugawara::analyzer::details
