#pragma once

#include <memory>

#include <takatori/type/data.h>
#include <takatori/util/sequence_view.h>

namespace yugawara::function::utils {

[[nodiscard]] bool each_is_widening_convertible(
        ::takatori::util::sequence_view<std::shared_ptr<::takatori::type::data const> const> as,
        ::takatori::util::sequence_view<std::shared_ptr<::takatori::type::data const> const> bs) noexcept;

} // namespace yugawara::function::utils
