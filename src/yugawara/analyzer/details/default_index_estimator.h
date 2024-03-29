#pragma once

#include <yugawara/analyzer/index_estimator.h>

namespace yugawara::analyzer::details {

/**
 * @brief a default implementation of storage::estimator.
 */
class default_index_estimator final : public index_estimator {
public:
    [[nodiscard]] result operator()(
            storage::index const& index,
            ::takatori::util::sequence_view<search_key const> search_keys,
            ::takatori::util::sequence_view<sort_key const> sort_keys,
            ::takatori::util::sequence_view<column_ref const> values) const override;
};

} // namespace yugawara::analyzer::details