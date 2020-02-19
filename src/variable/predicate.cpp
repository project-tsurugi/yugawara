#include <yugawara/variable/predicate.h>

namespace yugawara::variable {

bool operator==(predicate const& a, predicate const& b) noexcept {
    return a.equals(b);
}

bool operator!=(predicate const& a, predicate const& b) noexcept {
    return !(a == b);
}

std::ostream& operator<<(std::ostream& out, predicate const& value) {
    return value.print_to(out);
}

} // namespace yugawara::variable
