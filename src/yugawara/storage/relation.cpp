#include <yugawara/storage/relation.h>

namespace yugawara::storage {

using ::takatori::util::optional_ptr;

optional_ptr<provider const> relation::owner() const noexcept {
    return optional_ptr { owner_ };
}

std::ostream& operator<<(std::ostream& out, relation const& value) {
    return value.print_to(out);
}

} // namespace yugawara::storage
