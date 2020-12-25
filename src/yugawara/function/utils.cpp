#include "utils.h"

#include <yugawara/type/conversion.h>

namespace yugawara::function::utils {

bool each_is_widening_convertible(
        ::takatori::util::sequence_view<std::shared_ptr<::takatori::type::data const> const> as,
        ::takatori::util::sequence_view<std::shared_ptr<::takatori::type::data const> const> bs) noexcept {
    if (as.size() != bs.size()) {
        return false;
    }
    for (std::size_t i = 0, n = as.size(); i < n; ++i) {
        auto&& a = *as[i];
        auto&& b = *bs[i];
        if (type::is_widening_convertible(a, b) != true) {
            return false;
        }
    }
    return true;
}

} // namespace yugawara::function::utils
